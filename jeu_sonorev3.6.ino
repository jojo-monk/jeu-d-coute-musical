#include "Arduino.h"
//#include "SoftwareSerial.h"
#include <NeoSWSerial.h>
#include "DFRobotDFPlayerMini.h"
#include <SPI.h>
#include <MFRC522.h>
#include "Button2.h"
#include <EncoderButton.h>


#define SS_PIN 10
#define RST_PIN 9
// 4 jeux d'écoutes :
// 1 Jeu de reconnaissance d'instruments avec 20 cartes RFID, 2 modes :
// mode facile : 1 instrument seul est entendu (dossier 1 à 20)
// mode difficile : reconnaitre l'instrument dans un duo ou trio,
// ou extraits d'oeuvres(dossier 21 à 40)
// 20 instruments à trouver, possibilité d'en rajouter en rajoutant d'autres cartes
// sdcard : chaque dossier correspond à 1 instrument,
//
// 2 jeu de reconnaissance des hauteurs. 4 cartes (de 0 à 3), grave, aigu,
// glissando montant et descendant.
// mode facile : 1 instrument seul (dossiers 41 à 44)
// mode difficile : extraits d'oeuvres (dossiers 45 à 48)
//
// 3 jeu de reconnaissance des nuances. 4 cartes (de 4 à 7), piano, forte, crescendo, decrescendo
// mode facile : 1 instrument seul (dossiers 49 à 52)
// mode difficile : extraits d'oeuvres (dossiers 53 à 56)
//
// 4 jeu de reconnaissance de tempo. 4 cartes (8 à 11), lent, rapide, accélération, ralenti
// mode facile : 1 instrument seul (dossiers 57 à 60)
// mode difficile : extraits d'oeuvres (dossiers 61 à 64).
const byte rotaryPinA = A1;
const byte rotaryPinB = A0;
const byte swEncoderPin = A2;
const byte redLedPin = 5;
const byte greenLedPin = 6;
const byte interPin = 4;
const byte buttonPin = 8;
const byte nbJeu = 4;
const byte ledJeuPin[nbJeu] = { 7, A5, A4, A3 };

EncoderButton selectJeu(rotaryPinA, rotaryPinB, swEncoderPin);
Button2 interMode;
Button2 nextButton;
//SoftwareSerial mySoftwareSerial(2, 3); // RX, TX
NeoSWSerial mySoftwareSerial(2, 3);
DFRobotDFPlayerMini MP3Player;
MFRC522 rfid(SS_PIN, RST_PIN);

// sometime ACK messages sent on software serial seems to create bugs when trying to access to number of files in folders... strange but observed.
// if you get "no file found" too often, then try to set the flag to false.
const bool useMP3Ack = true;
// sometime you should try to disable reset flag, if your SD card isn't recognized at boot... strange but observed.
// if the MP3 does not start properly at boot, then try to set the flag to false.
const bool resetMP3OnBoot = true;
// Set volume value (0~30).
const byte MP3Volume = 25;

enum state { initial, lecture, ecoute, attente, rejouer, arret, annonce };
enum ex { instrus, hauteurs, nuance, tempo };

ex volatile choixJeu = instrus;
state volatile jeuState = annonce;

bool dbClk = false;
// mode facile = false (morceaux puisés dans les dossiers faciles)
// mode difficile = true (morceaux puisés dans les dossiers difficiles)
bool volatile mode;
bool volatile mp3End = false;
int volatile jeuFlag = 2;
int volatile songIndex = 0;
byte volatile songNb = 0;
byte nbSongTotal;
int folderIndex = -1;
byte volatile folderNb;
byte lastSongNb;
byte lastFolderNb;
const byte nbCardTotal = 20;
const byte nbCardInst = 20;
const byte nbCardHNT = 4;
//const byte nbCardHaut = 4;
//const byte nbCardNuance = 4;
//const byte nbCardTempo = 4;
const byte nbFolderTotal = (nbCardInst * 2) + ((nbCardHNT * 3) * 2);
// 90 : bonne réponse, 91 : mauvaise réponse
const byte jingle[2] = { 90, 91 };
const byte jeuTitre[nbJeu] = { 92, 93, 94, 95 };
const byte nbSongMax = 20;
byte selecTemp;
byte volatile annonceFlag = 0;
byte mp3Count[nbFolderTotal];
bool ledJeu[nbJeu] = { false, false, false, false };
int volatile reponse = -1;
byte volatile shiftDifficile;

byte nbCards[nbJeu] = { nbCardTotal, nbCardHNT, nbCardHNT, nbCardHNT };

struct playlist {
  byte instru[nbCardTotal];
  byte hauteur[nbCardHNT];
  byte nuance[nbCardHNT];
  byte tempo[nbCardHNT];
  byte songlist[nbSongMax];

};

playlist volatile play;

struct jeu {
  byte uid[4];
  byte folder[4];
};

const jeu cards[20] = {
  { { 170, 104, 79, 25 }, { 1, 21, 41, 45 } },
  { { 58, 13, 146, 25 }, { 2, 22, 42, 46 } },
  { { 26, 254, 81, 25 }, { 3, 23, 43, 47 } },
  { { 58, 99, 82, 25 }, { 4, 24, 44, 48 } },
  { { 10, 120, 151, 25 }, { 5, 25, 49, 53 } },
  { { 234, 49, 76, 25 }, { 6, 26, 50, 54 } },
  { { 58, 13, 74, 25 }, { 7, 27, 51, 55 } },
  { { 154, 177, 135, 25 }, { 8, 28, 52, 56 } },
  { { 26, 112, 77, 25 }, { 9, 29, 57, 61 } },
  { { 186, 99, 137, 25 }, { 10, 30, 58, 62 } },
  { { 212, 141, 149, 42 }, { 11, 31, 59, 63 } },
  { { 73, 191, 12, 3 }, { 12, 32, 60, 64 } },
  { { 121, 150, 253, 2 }, { 13, 33, 0, 0 } },
  { { 57, 78, 7, 3 }, { 14, 34, 0, 0 } },
  { { 105, 21, 247, 2 }, { 15, 35, 0, 0 } },
  { { 233, 119, 235, 2 }, { 16, 36, 0, 0 } },
  { { 73, 187, 49, 3 }, { 17, 37, 0, 0 } },
  { { 89, 163, 237, 2 }, { 18, 38, 0, 0 } },
  { { 137, 219, 31, 3 }, { 19, 39, 0, 0 } },
  { { 57, 201, 30, 3 }, { 20, 40, 0, 0 } },
};

byte newCard[4] = { 0, 0, 0, 0 };

// renvoie le n° de dossier de la playlist en fonction du jeu choisi
byte selecPlaylist(byte index) {
  switch (choixJeu) {
    case instrus:
      Serial.print("instrus: ");
      Serial.println(play.instru[index]);
      return play.instru[index];
      break;
    case hauteurs:
      Serial.print("hauteurs: ");
      Serial.println(play.hauteur[index]);
      return play.hauteur[index];
      break;
    case nuance:
      Serial.print("nuance: ");
      Serial.println(play.nuance[index]);
      return play.nuance[index];
      break;
    case tempo:
      Serial.print("tempo: ");
      Serial.println(play.tempo[index]);
      return play.tempo[index];
      break;
  }
}

// Annonce du titre du jeu
void titre() {
  annonceFlag = 0;
  //jeuFlag = 2;
  jeuState = annonce;
  MP3Player.playMp3Folder(jeuTitre[choixJeu]);
  delay(2000);
  MP3Player.pause();
}


// mélange aléatoire sans répétition de la liste des dossiers de la sdcard
void shuffle(bool all = false) {
  if (all) {
    for (int i = 0; i < nbCardTotal; ++i) {
      int index = random(i, nbCardTotal);
      int temp = play.instru[i];
      play.instru[i] = play.instru[index];
      play.instru[index] = temp;
    }
    for (int i = 0; i < nbCardHNT; ++i) {
      int index = random(i, nbCardHNT);
      int temp = play.hauteur[i];
      play.hauteur[i] = play.hauteur[index];
      play.hauteur[index] = temp;
    }
    for (int i = 0; i < nbCardHNT; ++i) {
      int index = random(i, nbCardHNT);
      int temp = play.nuance[i];
      play.nuance[i] = play.nuance[index];
      play.nuance[index] = temp;
    }
    for (int i = 0; i < nbCardHNT; ++i) {
      int index = random(i, nbCardHNT);
      int temp = play.tempo[i];
      play.tempo[i] = play.tempo[index];
      play.tempo[index] = temp;
    }
  }
  else {
    switch (choixJeu) {
      case instrus:
        for (int i = 0; i < nbCardTotal; ++i) {
          int index = random(i, nbCardTotal);
          int temp = play.instru[i];
          play.instru[i] = play.instru[index];
          play.instru[index] = temp;
        }
        break;
      case hauteurs:
        for (int i = 0; i < nbCardHNT; ++i) {
          int index = random(i, nbCardHNT);
          int temp = play.hauteur[i];
          play.hauteur[i] = play.hauteur[index];
          play.hauteur[index] = temp;
        }
        break;
      case nuance:
        for (int i = 0; i < nbCardHNT; ++i) {
          int index = random(i, nbCardHNT);
          int temp = play.nuance[i];
          play.nuance[i] = play.nuance[index];
          play.nuance[index] = temp;
        }
        break;
      case tempo:
        for (int i = 0; i < nbCardHNT; ++i) {
          int index = random(i, nbCardHNT);
          int temp = play.tempo[i];
          play.tempo[i] = play.tempo[index];
          play.tempo[index] = temp;
        }
        break;
    }
  }
}

// mélange aléatoire sans répétition des mp3 contenus dans 1 dossier, 20 fichiers max par dossier
void shuffleSong() {
  for (int i = 0; i < nbSongMax; ++i) {
    int index = random(i, nbSongMax);
    int temp = play.songlist[i];
    play.songlist[i] = play.songlist[index];
    play.songlist[index] = temp;
  }
}


// choix dossier suivant dans la playlist, si mode difficile, choix d'un fichier dans le dossier difficile
// si mode facile, choix dans le dossier facile.
//(N°dossier difficile = N°dossier facile + nb de cartes réponses du jeu selectionné).
// mélange aléatoire de la playlist lorsque tous les dossiers ont été parcouru 1 fois.
void getRandomFile() {
  ++folderIndex;
  if (folderIndex == nbCards[choixJeu]) {
    folderIndex = 0;
    ++songIndex;
    shuffle();
  }
  if (songIndex == nbSongMax) {
    songIndex = 0;
    shuffleSong();
  }
  folderNb = selecPlaylist(folderIndex) + shiftDifficile;
  if (mp3Count[folderNb - 1] == 0) {
    Serial.print(F("pas de mp3 dans le dossier "));  // pas de Mp3 dans le dossier difficile,
    Serial.println(folderNb);                   // on prends le dossier facile
    folderNb = selecPlaylist(folderIndex);
    if (mp3Count[folderNb - 1] == 0) {
      Serial.print(F("pas de mp3 dans le dossier "));
      Serial.println(folderNb);
      jeuState = initial;
      // pas de mp3 dans le dossier facile, on passe au morceau suivant.
    }
  }

  songNb = play.songlist[songIndex];
  songNb = map(songNb, 1, nbSongMax, 1, mp3Count[folderNb - 1]);
  //Serial.println(songNb);
  Serial.print(F(" dossier n° "));
  Serial.println(folderNb);
  Serial.print(F(" mp3 n° "));
  Serial.println(songNb);
  lastFolderNb = folderNb;
  lastSongNb = songNb;
}

// led verte allumée si reponse juste, led rouge si réponse fausse
void allumeLed(bool rep) {
  if (rep) {
    digitalWrite(greenLedPin, HIGH);
    digitalWrite(redLedPin, LOW);
  }
  else {
    digitalWrite(redLedPin, HIGH);
    digitalWrite(greenLedPin, LOW);
  }
}

// extinction des leds
void stopLed() {
  digitalWrite(greenLedPin, LOW);
  digitalWrite(redLedPin, LOW);
}

void jeuLed(byte nbJeu) {
  for (int i = 0; i < 4; ++i) {
    if (i == nbJeu) {
      ledJeu[i] = true;
    }
    else {
      ledJeu[i] = false;
    }
    digitalWrite(ledJeuPin[i], ledJeu[i]);
  }
}

void sonReponse(bool rep, byte mp3Nb = 0) {
  if (rep) {
    MP3Player.playMp3Folder(jingle[0]);
    delay(2000);
    MP3Player.playMp3Folder(mp3Nb);
    delay(2000);
  }
  else {
    MP3Player.playMp3Folder(jingle[1]);
    delay(2000);
  }
}

// Vérifie si la carte correspond au dossier de la playlist, et donc à l'instrument entendu.
void jeu(int carte) {
  byte verifRep = 0;
  for (int i = 0; i < 4; ++i) {
    if (cards[carte].folder[i] == lastFolderNb) {
      ++verifRep;
    }
  }
  if (verifRep != 0) {
    mp3End = false;
    Serial.println(F("Bravo, Gagné "));
    allumeLed(1);
    sonReponse(1, lastFolderNb - shiftDifficile);
    jeuState = annonce;
    //jeuFlag = 2;
    annonceFlag = 2;
  }
  else {
    mp3End = false;
    Serial.println(F("perdu"));
    allumeLed(0);
    sonReponse(0);
    jeuState = annonce;
    //jeuFlag = 2;
    annonceFlag = 1;
  }
  reponse = -1;
}


// récupère l'UID de la carte posée, et renvoie le n° de carte correspondant
void readRfid() {
  byte verif = 0;
  byte testCarte;
  if (rfid.PICC_IsNewCardPresent()) { // new tag is available
    if (rfid.PICC_ReadCardSerial()) { // NUID has been readed
      MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
      //byte card = rfid.uid.uidByte;
      //Serial.println(card);
      for (int i = 0; i < rfid.uid.size; i++) {
        newCard[i] = rfid.uid.uidByte[i];
        //Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print("uid carte : ");
        Serial.println(newCard[i]);
      }
      for ( int n = 0; n < nbCardTotal; n++) {
        for (int c = 0; c < 4; ++c ) {
          //testCarte = cards[n].uid[c];
          if (newCard[c] == cards[n].uid[c]) {
            ++verif;
          }
        }
        if (verif == 4) {
          reponse = n;
          Serial.println("carte trouvée ");
          Serial.print("carte n° ");
          Serial.println(reponse);
          verif = 0;
        }
        else {
          verif = 0;
        }
      }
      rfid.PICC_HaltA(); // halt PICC
      rfid.PCD_StopCrypto1(); // stop encryption on PCD
    }
  }
  for (int i = 0; i < 4; ++i) {
    newCard[i] = 0;
  }
}

// gestion de l'interrupteur de mode difficile ou facile
// décalage du nb de cartes pour puiser dans les dossiers difficiles
void boutonMode() {
  int valeurInter = digitalRead(interPin);
  if ( valeurInter == LOW) {
    mode = true;
    Serial.println(F("mode difficile"));
    switch (choixJeu) {
      case instrus:
        shiftDifficile = nbCardInst;
        break;
      case hauteurs:
        shiftDifficile = nbCardHNT;
        break;
      case nuance:
        shiftDifficile = nbCardHNT;
        break;
      case tempo:
        shiftDifficile = nbCardHNT;
        break;
    }
  }
  else {
    mode = false;
    Serial.println(F("mode facile"));
    shiftDifficile = 0;
  }
}

void boutonSuivant() {
  if (dbClk == false) {
    mp3End = false;
    Serial.println(F("suivant"));
    jeuState = initial;
    rfid.PCD_Init(); // init MFRC522
  }
}

void boutonPause() {
  if (dbClk == false) {
    MP3Player.pause();
    Serial.println(F("On fait une pause"));
    jeuState = arret;
    dbClk = true;
  }
  else {
    Serial.println(F("On reprend"));
    jeuState = initial;
    dbClk = false;
    rfid.PCD_Init();
  }
}

void boutonSelect(EncoderButton& eb) {
  Serial.println(F("bouton selection appuyé"));
  choixJeu = selecTemp;
  boutonMode();
  shuffle();
  shuffleSong();
  folderIndex = -1;
  songIndex = 0;
  jeuState = initial;
  dbClk = false;
  titre();
  rfid.PCD_Init();
}

void selection(EncoderButton& eb) {
  selecTemp = abs(eb.position() % nbJeu);
  Serial.println(selecTemp);
  jeuLed(selecTemp);
}


// pour savoir si le mp3 est fini ou non,
void printDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      //mp3End = true;
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerUSBInserted:
      Serial.println(F("USB Inserted!"));
      break;
    case DFPlayerUSBRemoved:
      Serial.println(F("USB Removed!"));
      break;
    case DFPlayerPlayFinished:
      //      Serial.print(F("Number:"));
      //      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      mp3End = true;
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
    default:
      mp3End = false;
      break;
  }
}

// pour savoir si le mp3 est fini ou non.
void mp3State() {
  while (MP3Player.available()) {
    printDetail(MP3Player.readType(), MP3Player.read()); //Print the detail message from DFPlayer to handle different errors and states.
  }
}


void setup() {
  mySoftwareSerial.begin(9600);
  Serial.begin(9600);
  Serial.println(F("jeu sonore v3"));
  randomSeed(analogRead(A6));
  SPI.begin(); // init SPI bus
  rfid.PCD_Init(); // init MFRC522
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  for (int i = 0; i < nbJeu; ++i) {
    pinMode(ledJeuPin[i], OUTPUT);
  }

  // Use softwareSerial to communicate with mp3,
  if ( !MP3Player.begin(mySoftwareSerial, useMP3Ack, resetMP3OnBoot) )
  {
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    Serial.println(F("3.If it still fail : try to change MP3 flags in configuration"));
    while (true) {
      delay(0);
    }
  }
  Serial.println(F("MP3 player ready"));
  MP3Player.setTimeOut(800);
  //MP3Player.pause();
  MP3Player.volume(MP3Volume);
  MP3Player.EQ(DFPLAYER_EQ_NORMAL);  // Set EQ
  MP3Player.outputDevice(DFPLAYER_DEVICE_SD);   // Set device we use : SD card
  delay(2000);
  for (int i = 0; i < nbFolderTotal; ++i) {
    mp3Count[i] = MP3Player.readFileCountsInFolder(i + 1);
    //mp3Count[i] = MP3Player.numTracksInFolder(i + 1);
    Serial.print(F(" dossier n° : "));
    Serial.println(i + 1);
    Serial.print(F("nb de fichiers : "));
    Serial.println(mp3Count[i]);
    nbSongTotal += mp3Count[i];
    byte temoin = map(i, 0, nbFolderTotal, 0, 4); //témoin de chargement
    digitalWrite(ledJeuPin[temoin], HIGH);
  }
  Serial.print(F(" nb total de mp3 : "));
  Serial.println(nbSongTotal);
  interMode.begin(interPin);
  nextButton.begin(buttonPin);
  interMode.setChangedHandler(boutonMode);
  nextButton.setClickHandler(boutonSuivant);
  nextButton.setDoubleClickHandler(boutonPause);
  selectJeu.setClickHandler(boutonSelect);
  selectJeu.setEncoderHandler(selection);
  boutonMode();
  //delay(150);
  for (byte i = 0; i < nbJeu; ++i) {
    digitalWrite(ledJeuPin[i], LOW);
  }
  // animation visuelle pour faire joli au démarrage
  for (int i = 0; i < nbJeu; ++i) {
    digitalWrite(ledJeuPin[i], HIGH);
    delay(150);
    digitalWrite(ledJeuPin[i], LOW);
    delay(150);
  }
  digitalWrite(greenLedPin, HIGH);
  delay(150);
  digitalWrite(greenLedPin, LOW);
  delay(150);
  digitalWrite(redLedPin, HIGH);
  delay(150);
  digitalWrite(redLedPin, LOW);
  delay(150);
  for (int i = 0; i < nbJeu; ++i) {
    digitalWrite(ledJeuPin[i], HIGH);
  }
  digitalWrite(greenLedPin, HIGH);
  digitalWrite(redLedPin, HIGH);
  delay(150);
  for (int i = 0; i < nbJeu; ++i) {
    digitalWrite(ledJeuPin[i], LOW);
  }
  digitalWrite(greenLedPin, LOW);
  digitalWrite(redLedPin, LOW);
  jeuLed(choixJeu);
  for (int i = 0; i < nbCardTotal; ++i) {
    play.instru[i] = i + 1;
  }
  //shuffle(instrus);
  for (int i = 0; i < nbCardHNT; ++i) {
    play.hauteur[i] = i + 41;
  }
  for (int i = 0; i < nbCardHNT; ++i) {
    play.tempo[i] = i + 57;
  }
  for (int i = 0; i < nbCardHNT; ++i) {
    play.nuance[i] = i + 49;
  }
  for (int i = 0; i < nbSongMax; ++i) {
    play.songlist[i] = i + 1;
  }
  shuffle(true);
  shuffleSong();
  titre();


}

void loop() {
  nextButton.loop();
  switch (jeuState) {
    case initial:
      // initialise les variables
      //Serial.println("initial");
      stopLed();
      jeuLed(choixJeu);
      mp3End = false;
      reponse = -1;
      MP3Player.pause();
      getRandomFile();
      jeuState = lecture;
      break;
    case lecture:
      // lance la lecture du Mp3 et se met en attente de réponse à la fin du mp3
      //Serial.println("lecture");
      //Serial.println(folderNb);
      MP3Player.playFolder(folderNb, songNb);
      delay(200);
      jeuState = ecoute;
      break;
    case ecoute:
      // en cours d'écoute
      //Serial.println("ecoute");
      mp3State();
      if (mp3End) {
        Serial.println(F("mp3 fini"));
        if (jeuFlag == 1) {
          jeuState = initial;
        }
        if (jeuFlag == 0) {
          jeuState = rejouer;
        }
        if (jeuFlag == -1) {
          jeuState = attente;
        }
        if (jeuFlag == 2) {
          jeuState = annonce;
        }
      }
      break;
    case rejouer:
      // si une mauvaise réponse est donnée, on a droit à un autre essai
      // rejoue le dernier mp3 joué
      //Serial.println("rejouer");
      stopLed();
      mp3End = false;
      jeuFlag = -1;
      reponse = -1;
      MP3Player.playFolder(lastFolderNb, lastSongNb);
      delay(200);
      jeuState = ecoute;
      break;
    case attente:
      // attente de réponse, et vérifie si la réponse est bonne ou pas
      //Serial.println("attente");
      //MP3Player.pause();
      readRfid();
      if (reponse != -1) {
        jeu(reponse);
      }
      break;
    case arret:
      selectJeu.update();
      interMode.loop();
      //Serial.println(F("on fait une pause"));
      break;
    case annonce:
    //Serial.println("annonce");
    
      if (annonceFlag == 0) {
        //Serial.println("annonce vocale : Titre");
        jeuFlag = -1;
        jeuState = initial;
      }
      if (annonceFlag == 2) {
        //Serial.println("annonce vocale : gagné");
        jeuFlag = -1;
        jeuState = initial;
      }
      if (annonceFlag == 1) {
        //Serial.println("annonce jingle : perdu");
        jeuFlag = 0;
        jeuState = rejouer;
      }
      break;
  }

}
