/***********************************************************************
               Objet decodeur de teleinformation client (TIC)
               format Linky "historique" ou anciens compteurs
               electroniques.

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
VTIC	02	J
DATE	H190323161039	G
NGTF	      B00000000	*
EASF10	000000000	"
EASD01	000350847	;
EASD02	000000000	!
EASD03	000000000	"
EASD04	000000000	#
IRMS1	001	/
URMS1	226	D
PREF	06	E
PCOUP	06	_
SINSTS	00129	R
SMAXSN	H190323120831	01748	B
SMAXSN-1	H190322073403	02624	[
CCASN	H190323160000	00098	E
CCASN-1	H190323153000	00084	 
UMOY1	H190323161000	226	2
STGE	003A0001	:
MSG1	PAS DE          MESSAGE         	<
PRM	21376700393040	.
RELAIS	000	B
NTARF	01	N
NJOURF	00	&
NJOURF+1	00	B
PJOURF+1	00008001 NONUTILE NONUTILE NONUTILE NONUTILE NONUTILE

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



  
//#ifdef LINKYDEBUG
      //Serial << _pDec << endl;
//#endif


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
  #ifdef LINKYDEBUG
      Serial << LIne << endl;
  #endif
  

  unsigned int checksum = 0;
  int SizeOfArray = strlen(MyArray);
  for(int x = 0; x < SizeOfArray-1; x++)
	{
      	if (MyArray[x] != ' ')
      	{
          #ifdef LINKYDEBUG
            Serial << MyArray[x];
          #endif
          checksum += MyArray[x];
      	} else {
          #ifdef LINKYDEBUG
            Serial << "space ";
          #endif          	
          checksum += 0x09;
      	}
  }
  checksum = (checksum & 0x3F) + 0x20; //Functionally equivalent to - checksum = (checksum % 0x40) + 0x20;
  char aChar = 0 + checksum;
  if (aChar == MyArray[SizeOfArray-1]){
    #ifdef LINKYDEBUG
      Serial << "--> CHECKSUM SUCCESS\n";
    #endif    
    return 0; // return a checksum success
	} else {
    #ifdef LINKYDEBUG
      Serial << "Character is %c-%c\n", aChar, MyArray[SizeOfArray-1]; //RISKRISK
    #endif 
    return -1; // return a failed checksum
  }

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
