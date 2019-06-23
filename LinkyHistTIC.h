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

***********************************************************************/
#ifndef _LinkyHistTIC
#define _LinkyHistTIC true

#include <SoftwareSerial.h>

/******************************** Class *******************************
      LinkyHistTIC : Linky historique ITIC (t�l�information client)

Reference : ERDF-NOI-CPT_54E V1
***********************************************************************/
#define CLy_BfSz 20       /* Maximum size of the Rx buffers */



class LinkyHistTIC
  {
  public:
    LinkyHistTIC(uint8_t pin_Rx, uint8_t pin_Tx);    /* Constructor */

    void Init();        /* Initialisation, call from setup() */
    void Update();      /* Update, call from loop() */

    String content;

    
    

    bool baseIsNew();   /* Returns true if base has changed */
    bool iinstIsNew();  /* Returns true if iinst has changed */
    bool pappIsNew();   /* Returns true if papp has changed */

    uint32_t EASTv();    /* Returns base index in Wh */
    uint32_t SINSTSv();    /* Returns iinst in A */
    
        
    int isCheckSumOk(char* LIne);
    int process(char* LIne);
    int strToUL(char* str);

  private:
    enum GroupId:uint8_t {CLy_base, CLy_iinst, CLy_papp};

    char _BfA[CLy_BfSz];        /* Buffer A */
    char _BfB[CLy_BfSz];        /* Buffer B */

    uint8_t _FR;                /* Flag register */
    uint8_t _DNFR;              /* Data new flag register */



    
    uint32_t _base;
    uint8_t _iinst;
    uint16_t _papp;

    SoftwareSerial _LRx;   /* Needs to be constructed at the same time
                            * as LinkyHistTIC. Cf. Special syntax
                            * (initialisation list) in LinkyHistTIC
                            * constructor */

    char *_pRec;    /* Reception pointer in the buffer */
    char *_pDec;    /* Decode pointer in the buffer */
    uint8_t _iRec;   /*  Received char index */
    uint8_t _iCks;   /* Index of Cks in the received message */
    uint8_t _GId;    /* Group identification */

    uint8_t _pin_Rx;
    uint8_t _pin_Tx;

  };
  
#endif
/*************************** End of code ******************************/
