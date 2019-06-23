/***********************************************************************
               Objet decodeur de teleinformation client (TIC)
               format Linky "historique" ou anciens compteurs
               electroniques.

  Lit les trames et decode les groupes :
  BASE  : (base) index general compteur en Wh,
  IINST : (iinst) intensit� instantan�e en A,
  PAPP  : (papp) puissance apparente en VA.

  Reference : ERDF-NOI-CPT_54E V1

  V06 : MicroQuettas mars 2018
  V07 : DPegon June 10th
    What works - it spits out converted values for EAST and SINST

    What needs improving, the checksums don't work, I haven't bothered sorting out why
    - there is a bunch of ghost code (from the previous iteration which could do with clearing out bit by bit
    - I'd like to return the values for SINSTS and EAST to the main function so that I could ask it to sleep nicely

***********************************************************************/

/***************************** Includes *******************************/
#include <string.h>
#include <Streaming.h>
#include "LinkyHistTIC.h"

#include "EspMQTTClient.h"

#define LINKYDEBUG false

/***********************************************************************
                  Objet récepteur TIC Linky historique

  Trame historique :
  - délimiteurs de trame :     <STX> trame <ETX>
    <STX> = 0x02
    <ETX> = 0x03

  - groupes dans une trame : <LF>lmnopqrs<SP>123456789012<SP>C<CR>
    <LR> = 0x0A
    lmnopqrs = label 8 char max
    <SP> = 0x20 ou 0x09 (<HT>)
    123456789012 = data 12 char max
    C = checksum 1 char
    <CR> = 0x0d
       Longueur max : label + data = 7 + 9 = 16

  _FR : flag register

    |  7  |  6  |   5  |   4  |   3  |  2  |   1  |   0  |
    |     |     | _Dec | _GId | _Cks |     | _RxB | _Rec |

     _Rec : receiving
     _RxB : receive in buffer B, decode in buffer A
     _GId : Group identification
     _Dec : decode data

  _DNFR : data available flags

    |  7  |  6  |  5  |  4  |  3  |   2   |   1    |   0   |
    |     |     |     |     |     | _papp | _iinst | _base |

  Exemple of group :
       <LF>lmnopqr<SP>123456789<SP>C<CR>
           0123456  7 890123456  7 8  9
     Cks:  xxxxxxx  x xxxxxxxxx    ^
                                   |  iCks
    Minimum length group  :
           <LF>lmno<SP>12<SP>C<CR>
               0123  4 56  7 8  9
     Cks:      xxxx  x xx    ^
                             |  iCks

    The storing stops at CRC (included), ie a max of 19 chars

***********************************************************************/

/****************************** Macros ********************************/
#ifndef SetBits
#define SetBits(Data, Mask) \
  Data |= Mask
#endif

#ifndef ResetBits
#define ResetBits(Data, Mask) \
  Data &= ~Mask
#endif

#ifndef InsertBits
#define InsertBits(Data, Mask, Value) \
  Data &= ~Mask;\
  Data |= (Value & Mask)
#endif

#ifndef P1
#define P1(name) const char name[] PROGMEM
#endif

/***************************** Defines ********************************/
#define CLy_Bds 9600      /* Transmission speed in bds */

/* Number of consecutive characters to read (max = 255) */
const unsigned int BUFFER_SIZE = 255;
/* Max number of characters in one  (<= BUFFER SIZE) */
const unsigned int MAX_LINE_SIZE = 150;
/* Min number of characters in one  (<= MAX__SIZE) */
const unsigned int MIN_LINE_SIZE = 5;
const char DEMILITER = 0x09;

typedef struct {
  unsigned int EAST;
  //unsigned int EAIT;
  unsigned int SINSTS;
  //unsigned int SINSTI;
} Teleinfo;

Teleinfo teleinfo = { .EAST = 0, .SINSTS = 0 };


#define bLy_Rec   0x01     /* Receiving */
#define bLy_RxB   0x02     /* Receive in buffer B */
#define bLy_Cks   0x08     /* Check Cks */
#define bLy_GId   0x10     /* Group identification */
#define bLy_Dec   0x20     /* Decode */

#define bLy_base  0x01     /* BASE group available */
#define bLy_iinst 0x02     /* IINST group availble */
#define bLy_papp  0x04     /* PAPP group available */

#define Car_SP    0x20     /* Char space */
#define Car_HT    0x09     /* Horizontal tabulation */

#define CLy_MinLg  8      /* Minimum usefull message length */




const char CLy_Sep[] = {Car_SP, Car_HT, '\0'};  /* Separators */
int c_pos;

/************************* Données en progmem *************************/
P1(PLy_base)  = "BASE";    /* Rank 0 = CLy_base */
P1(PLy_iinst) = "IINST";   /* Rank 1 = CLy_iinst */
P1(PLy_papp)  = "PAPP";    /* Rank 2 = CLy_papp */

/*************** Constructor, methods and properties ******************/
LinkyHistTIC::LinkyHistTIC(uint8_t pin_Rx, uint8_t pin_Tx) \
: _LRx (pin_Rx, pin_Tx)  /* Software serial constructor
                                  Achtung : special syntax */
{
  _FR = 0;
  _DNFR = 0;
  _pRec = _BfA;    /* Receive in A */
  _pDec = _BfB;    /* Decode in B */
  _iRec = 0;
  _iCks = 0;
  _GId = CLy_base;
  _pin_Rx = pin_Rx;
  _pin_Tx = pin_Tx;
};

void LinkyHistTIC::Init()
{
  /* Initialise the SoftwareSerial */
  pinMode (_pin_Rx, INPUT);
  pinMode (_pin_Tx, OUTPUT);
  _LRx.begin(CLy_Bds);
}

void LinkyHistTIC::Update()
{ /* Called from the main loop */
  char c;


  uint8_t cks, i;
  uint32_t ba;
  uint16_t pa;
  bool Run = true;

  /* Achtung : actions are in the reverse order to prevent
               execution of all actions in the same turn of
               the loop */

  /* 1st part, last action : decode information */
  if (_FR & bLy_Dec)
  {
    ResetBits(_FR, bLy_Dec);     /* Clear requesting flag */
    _pDec = strtok(NULL, CLy_Sep);

    switch (_GId)
    {
      case CLy_base:
        ba = atol(_pDec);
        if (_base != ba)
        { /* New value for _base */
          _base = ba;
          SetBits(_DNFR, bLy_base);
        }
        break;

      case CLy_iinst:
        i = (uint8_t) atoi(_pDec);
        if (_iinst != i)
        { /* New value for _iinst */
          _iinst = i;
          SetBits(_DNFR, bLy_iinst);
        }
        break;

      case CLy_papp:
        pa = atoi(_pDec);
        if (_papp != pa)
        { /* New value for papp */
          _papp = pa;
          SetBits(_DNFR, bLy_papp);
        }
        break;

      default:
        break;
    }
  }

  /* 2nd part, second action : group identification */
  if (_FR & bLy_GId)
  {
    ResetBits(_FR, bLy_GId);   /* Clear requesting flag */
    _pDec = strtok(_pDec, CLy_Sep);

    if (strcmp_P(_pDec, PLy_base) == 0)
    {
      Run = false;
      _GId = CLy_base;
    }

    if (Run && (strcmp_P(_pDec, PLy_iinst) == 0))
    {
      Run = false;
      _GId = CLy_iinst;
    }

    if (Run && (strcmp_P(_pDec, PLy_papp) == 0))
    {
      Run = false;
      _GId = CLy_papp;
    }

    if (!Run)
    {
      SetBits(_FR, bLy_Dec);   /* Next = decode */
    }
  }

  /* 3rd part, first action : check cks */
  if (_FR & bLy_Cks)
  {
    ResetBits(_FR, bLy_Cks);   /* Clear requesting flag */
    cks = 0;
    if (_iCks >= CLy_MinLg)
    { /* Message is long enough */
      for (i = 0; i < _iCks - 1; i++)
      {
        cks += *(_pDec + i);
      }
      cks = (cks & 0x3f) + Car_SP;

#ifdef LINKYDEBUG
      //Serial << _pDec << endl;
#endif

      if (cks == *(_pDec + _iCks))
      { /* Cks is correct */
        *(_pDec + _iCks - 1) = '\0';
        /* Terminate the string just before the Cks */
        SetBits(_FR, bLy_GId);  /* Next step, group identification */

#ifdef LINKYDEBUG
      }
      else
      {
        i = *(_pDec + _iCks);
        //Serial << F("Error Cks ") << cks << F(" - ") << i << endl;
#endif
      }   /* Else, Cks error, do nothing */

    }     /* Message too short, do nothing */
  }

  /* 4th part, receiver processing */
  while (_LRx.available())
  { /* At least 1 char has been received */
    c = _LRx.read() & 0x7f;   /* Read char, exclude parity */
    //c = _LRx.read();
    //Serial << c;
    content.concat(c);

// if we detect a new line
  if (((c >= 0x02) && (c <= 0x03)) || ((c >= 0x0a) && (c <= 0x0f)))
        {
          
          
          if (content.length() > MIN_LINE_SIZE)
          {
            //Serial << "new ------ ";
            //Serial << content << endl;

            int str_len = content.length() + 1; 
            char LIne[str_len];
            content.toCharArray(LIne, str_len);
            //Serial << LIne << endl;
            if ((LinkyHistTIC::isCheckSumOk(LIne)) == 0) {
                Serial << ("SUCCESS --> Checksum OK on : \"%s\"\n", LIne);
                
              } else {
                //Serial << "NOT OK";
                LinkyHistTIC::process(LIne);
              }
               LIne[0] = 0;
          }
          
    
          
          
          
          
          content = "";
         

        }

   


    if (_FR & bLy_Rec)
    { /* On going reception */
      if (c == '\r')
      { /* Received end of group char */
        ResetBits(_FR, bLy_Rec);   /* Receiving complete */
        SetBits(_FR, bLy_Cks);     /* Next check Cks */
        _iCks = _iRec - 1;         /* Index of Cks in the message */
        *(_pRec + _iRec) = '\0';   /* Terminate the string */

        /* Swap reception and decode buffers */
        if (_FR & bLy_RxB)
        { /* Receiving in B, Decode in A, swap */
          ResetBits(_FR, bLy_RxB);
          _pRec = _BfA;       /* --> Receive in A */
          _pDec = _BfB;       /* --> Decode in B */
        }
        else
        { /* Receiving in A, Decode in B, swap */
          SetBits(_FR, bLy_RxB);
          _pRec = _BfB;     /* --> Receive in B */
          _pDec = _BfA;     /* --> Decode in A */
        }

      }  /* End reception complete */
      else
      { /* Other character */
        *(_pRec + _iRec) = c; /* Store received character */
        _iRec += 1;
        if (_iRec >= CLy_BfSz - 1)
        { /* Buffer overrun */
          ResetBits(_FR, bLy_Rec); /* Stop reception and do nothing */
        }
      }  /* End other character than '\r' */
    }    /* End on-going reception */
    else
    { /* Reception not yet started */
      if (c == '\n')
      { /* Received start of group char */
        _iRec = 0;
        SetBits(_FR, bLy_Rec);   /* Start reception */
      }
    }
      }  /* End while */

}

int LinkyHistTIC::strToUL(char* str)
{
  unsigned int val = 0;
  char *endptr;
  val = strtoul(str, &endptr, 10);
 if (endptr == str)
  {
    Serial << ("ERROR --> Cannot convert to ul an empty string: \"%s\"\n", str);
    return -1;
  }
  return (int)val;
}

int LinkyHistTIC::process(char* LIne)
{
  
  char label [MAX_LINE_SIZE];
  char elem1 [MAX_LINE_SIZE];
  char elem2 [MAX_LINE_SIZE];
  int val;
  unsigned int start = 0;
  unsigned int i = 0;
  // Find next elem
  label[0] = '\0';
  for (i = start; i <= strlen(LIne) - 2; i++)
  {
    // Find the delimiter
    if (LIne[i] == DEMILITER)
    {
      memcpy(label, &LIne[start], i-start);
      label[i-start] = '\0';
      break;
    }
  }
  // Find next elem
  start = i+1;
  elem1[0] = '\0';
  for (i = start; i <= strlen(LIne) - 2; i++)
  {
    // Find the delimiter
    if (LIne[i] == DEMILITER)
    {
      memcpy(elem1, &LIne[start], i-start);
      elem1[i-start] = '\0';
      break;
    }
  }
  // Find next elem
  start = i+1;
  elem2[0] = '\0';
  for (i = start; i <= strlen(LIne) - 2; i++)
  {
    // Find the delimiter
    if (LIne[i] == DEMILITER)
    {
      memcpy(elem2, &LIne[start], i-start);
      elem2[i-start] = '\0';
      break;
    }
  }

  //Serial("DEBUG --> New : %s - %s - %s\n", label, elem1, elem2);
  // Save value
  if(strcmp(label, "EAST") == 0) {
    
    val = LinkyHistTIC::strToUL(elem1);
    Serial << "EAST found ---- " ;
    Serial << val << endl;
    if (val > 67) // Got some bogus values around 67 
    {
      teleinfo.EAST = (unsigned int)val;
      //client.publish("Linky/EAST", val, 1); // You can activate the retain flag by setting the third parameter to true
    }
    
    else return -1;
    } else if(strcmp(label, "SINSTS") == 0) {
    val = LinkyHistTIC::strToUL(elem1);
    Serial << "SINSTS found ---- " ;
    Serial << val << endl;
    if (val > 1) // Got some false values around 67 when it should be  
    {
      teleinfo.SINSTS = (unsigned int)val;
      //client.publish("Linky/SINSTS", val, 1); // You can activate the retain flag by setting the third parameter to true
    }
    else return -1;
  }
}

int LinkyHistTIC::isCheckSumOk(char* LIne)
{
  //Serial << LIne << endl;
  unsigned int checksum_expected = 0;
  unsigned int checksum_real = 0;
  unsigned int i;

  checksum_expected = LIne[strlen(LIne) - 1];
  for (i = 0; i <= strlen(LIne) - 2; i++)
  {
    checksum_real += LIne[i];
  }
  checksum_real = (checksum_real & 0x3F) + 0x20;

  if (checksum_expected != checksum_real)
  {
    //printf("ERROR --> Checksum failed on line: \"%s\"\n", LIne);
     char *p;
     int linelen = strlen(LIne);
    for (p = LIne; linelen-- > 0; p++)
      //printf(" 0x%x", *p);
    //printf("\n");
    return -1;
  }

  return 0;
}



bool LinkyHistTIC::pappIsNew()
{
  bool Res = false;

  if (_DNFR & bLy_papp)
  {
    Res = true;
    ResetBits(_DNFR, bLy_papp);
  }
  return Res;
}

uint32_t LinkyHistTIC::SINSTSv()
{
  return teleinfo.SINSTS;
}

uint32_t LinkyHistTIC::EASTv()
{
  return teleinfo.EAST;
}


/***********************************************************************
               Fin d'objet r�cepteur TIC Linky historique
***********************************************************************/
