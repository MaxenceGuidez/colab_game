// IMPORTS ------------------------------------------------------------------------------
#include "WiFi.h"
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"

// SERVERS/WIFI -------------------------------------------------------------------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char *ssid = "MyAccessPoint";
const char *password = "MyPassword";

// BUTTONS VARIABLES --------------------------------------------------------------------
const int btnGreenPin = 23;
int btnGreenState = 0;
int lastBtnGreenState = 0;

const int btnRedPin = 22;
int btnRedState = 0;
int lastBtnRedState = 0;

// LIMIT SWITCH VARIABLES ---------------------------------------------------------------
const int limitRightPin = 26;
int limitRightState = 0;
int lastLimitRightState = 0;

const int limitLeftPin = 33;
int limitLeftState = 0;
int lastLimitLeftState = 0;

// TOUCH VARIABLES ---------------------------------------------------------------
const int touchPin = 4;
const int touchThreshold = 20;
int touchValue;
int lastTouchValue;
bool isTouching = false;

// MOTOR VARIABLES ----------------------------------------------------------------------
const int dirPin = 19;
const int stepPin = 18;
const int stepsPerRev = 100;
const int enablePin = 13;

// DEBOUNCE VARIABLES -------------------------------------------------------------------
const int debounceDelay = 50;
unsigned long lastDebounceTimeBtnGreen = 0;
unsigned long lastDebounceTimeBtnRed = 0;
unsigned long lastDebounceTimeLimitRight = 0;
unsigned long lastDebounceTimeLimitLeft = 0;
unsigned long lastDebounceTimeTouch = 0;

// QUESTIONS VARIABLES ------------------------------------------------------------------
int actualQuestionIndex = 0;
bool isVictory = false;

enum Color {
  GREEN,
  RED
};

struct Question {
  String question;
  String greenDisplay;
  String redDisplay;
  Color answer;
};

Question questions[] = {
  {"Qui est le plus meurtier ?", "Requin", "Moustique", RED},
  {"Le langage de programmation Python a été nommé en l'honneur du serpent.", "Vrai", "Faux", GREEN},
  {"Le HTML est un langage de programmation.", "Vrai", "Faux", RED},
  {"Le protocole HTTP signifie 'Hypertext Transfer Protocol Secure'.", "Vrai", "Faux", RED},
  {"Le système d'exploitation Windows est basé sur le noyau Linux.", "Vrai", "Faux", RED},
  {"IPv6 est une version plus ancienne du protocole Internet par rapport à IPv4.", "Vrai", "Faux", RED},
  {"JavaScript peut être utilisé côté serveur avec Node.js.", "Vrai", "Faux", GREEN},
  {"L'intelligence artificielle (IA) est capable de penser et de ressentir comme un être humain.", "Vrai", "Faux", RED},
  {"Un octet a généralement 16 bits.", "Vrai", "Faux", RED},
  {"Un réseau local (LAN) couvre une grande région géographique, par exemple, un pays entier.", "Vrai", "Faux", RED},
  {"La programmation orientée objet (POO) est un paradigme de programmation basé sur des objets.", "Vrai", "Faux", GREEN},
  {"Un routeur est un périphérique utilisé pour connecter des ordinateurs à Internet.", "Vrai", "Faux", GREEN},
  {"Le stockage en nuage (cloud storage) permet de stocker des données localement sur un ordinateur.", "Vrai", "Faux", RED},
  {"Le protocole FTP (File Transfer Protocol) est utilisé pour envoyer des courriels.", "Vrai", "Faux", RED},
  {"Le terme 'bug' en informatique signifie toujours une petite créature nuisible.", "Vrai", "Faux", RED},
  {"Un algorithme est une série d'instructions pour résoudre un problème.", "Vrai", "Faux", GREEN},
  {"Le langage de programmation C++ est une version améliorée de C.", "Vrai", "Faux", GREEN},
  {"Les ordinateurs quantiques sont capables de résoudre tous les problèmes plus rapidement que les ordinateurs classiques.", "Vrai", "Faux", RED},
  {"Un fichier compressé au format ZIP ne peut contenir que des fichiers texte.", "Vrai", "Faux", RED},
  {"Le Wi-Fi est principalement utilisé pour la communication sans fil à longue distance.", "Vrai", "Faux", RED},
  {"La cybersécurité concerne la protection des systèmes informatiques contre les menaces en ligne.", "Vrai", "Faux", GREEN}
};

// GAME VARIABLES -----------------------------------------------------------------------
bool isGameStarted = false;

// OTHERS VARIABLES ---------------------------------------------------------------------
bool isMoving = false;
unsigned long lastMoveTime = 0;
const unsigned long timeout = 20000;

// FUNCTIONS WS -------------------------------------------------------------------------
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void notifyClients(String input) {
  Serial.print("NOTIFING CLIENTS : ");
  Serial.println(input);
  ws.textAll(String(input));
}

// OTHERS FUNCTION ----------------------------------------------------------------------
void initPins() {
  pinMode(btnGreenPin, INPUT_PULLUP);
  pinMode(btnRedPin, INPUT_PULLUP);
  
  pinMode(limitRightPin, INPUT_PULLUP);
  pinMode(limitLeftPin, INPUT_PULLUP);

  pinMode(dirPin, OUTPUT);
  pinMode(stepPin, OUTPUT);
  pinMode(enablePin, OUTPUT);

  digitalWrite(dirPin, LOW);
  digitalWrite(enablePin, LOW);
}

String generateJSONString(bool isVictory, const String &newQuestion, const String &newGreen, const String &newRed) {
  return R"(
    {
      "isLastVictory": ")" + String(isVictory) + R"(",
      "newQuestion": ")" + newQuestion + R"(",
      "newGreen": ")" + newGreen + R"(",
      "newRed": ")" + newRed + R"("
    }
  )";
}

void checkButton(int &btnState, int &lastBtnState, unsigned long &lastDebounceTimeBtn, int btnPin) {
  int newBtnState = !digitalRead(btnPin);

  if (newBtnState != lastBtnState) {
    lastDebounceTimeBtn = millis();
  }

  if ((millis() - lastDebounceTimeBtn) > debounceDelay) {
    if (newBtnState != btnState) {
      btnState = newBtnState;

      if (btnState == HIGH) {
        Color color;

        if (btnPin == btnGreenPin) color = GREEN;
        if (btnPin == btnRedPin) color = RED;

        if (questions[actualQuestionIndex].answer == color) isVictory = true;
        else isVictory = false;

        actualQuestionIndex++;

        String jsonStr = generateJSONString(
          isVictory,
          questions[actualQuestionIndex].question,
          questions[actualQuestionIndex].greenDisplay,
          questions[actualQuestionIndex].redDisplay
        );
        
        notifyClients(jsonStr);
        
        if (isVictory) {
          isMoving = true;
          digitalWrite(enablePin, LOW);
        }
      }
    }
  }
  lastBtnState = newBtnState;
}

void checkLimitSwitch(int &limitState, int &lastLimitState, unsigned long &lastDebounceTimeLimit, int limitPin) {
  int newLimitState = !digitalRead(limitPin);

  if (newLimitState != lastLimitState) {
    lastDebounceTimeLimit = millis();
  }

  if ((millis() - lastDebounceTimeLimit) > debounceDelay) {
    if (newLimitState != limitState) {
      limitState = newLimitState;

      if (limitState == HIGH) {
        isMoving = true;
        digitalWrite(enablePin, LOW);
      }
    }
  }
  lastLimitState = newLimitState;
}

void checkButtonsAndLimitSwitches() {
  checkButton(btnGreenState, lastBtnGreenState, lastDebounceTimeBtnGreen, btnGreenPin);
  checkButton(btnRedState, lastBtnRedState, lastDebounceTimeBtnRed, btnRedPin);

  checkLimitSwitch(limitRightState, lastLimitRightState, lastDebounceTimeLimitRight, limitRightPin);
  checkLimitSwitch(limitLeftState, lastLimitLeftState, lastDebounceTimeLimitLeft, limitLeftPin);
}

void activeMotor() {
  if (isMoving) {
    if (btnGreenState == HIGH) {
      moveForward(1);
    }
    if (btnRedState == HIGH) {
      moveForward(1);
    }
    if (limitRightState == HIGH || isTouching) {
      returnBeginning();
      isTouching = false;
    }
    if (limitLeftState == HIGH) {
      moveForward(0.5);
    }

    isMoving = false;
    lastMoveTime = millis();
  }
}

void moveForward(float times) {
  digitalWrite(dirPin, LOW);
  for (int i = 0; i < stepsPerRev * times; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(3000);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(3000);

    if (!digitalRead(limitRightPin) == HIGH) break;
  }
}

void returnBeginning() {
  digitalWrite(dirPin, HIGH);
  while (true) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(4000);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(4000);

    if (!digitalRead(limitLeftPin) == HIGH) {
      moveForward(0.5);
      break;
    }
  }
}

void checkTimeout() {
  if (!isMoving && (millis() - lastMoveTime) > timeout) {
    digitalWrite(enablePin, HIGH);
  }
}

void checkTouch() {
  int newTouchValue = touchRead(touchPin);

  if (newTouchValue != lastTouchValue) {
    lastDebounceTimeTouch = millis();
  }

  if ((millis() - lastDebounceTimeTouch) > debounceDelay) {
    if (newTouchValue != touchValue) {
      touchValue = newTouchValue;

      if(touchValue < touchThreshold) {
        isMoving = true;
        isTouching = true;

        isGameStarted = false;
        actualQuestionIndex = 0;
        notifyClients("RESET");
      }
    }
  }

  lastTouchValue = newTouchValue;
}

// SETUP / LOOP -------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  initPins();

  lastMoveTime = millis();

  // INIT WIFI/AP -----------------------------------------------------------------------
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("Adresse IP du point d'accès : ");
  Serial.println(WiFi.softAPIP());

  // WEB SERVER ROUTES ------------------------------------------------------------------
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <title>Chevaux</title>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
            html {
                font-family: Helvetica;
                display: inline-block;
                margin: 0 auto;
                text-align: center;
            }
            .button {
                background-color: #4CAF50;
                border: none;
                color: white;
                padding: 16px 40px;
                text-decoration: none;
                font-size: 30px;
                margin: 2px;
                cursor: pointer;
            }
        </style>
    </head>
    <body>
        <h1>Bienvenue !</h1>
        <p>Cliquez pour commencer</p>
        <p><a href="/start"><button class="button">COMMENCER</button></a></p>
    </body>
    </html>)";

    request->send(200, "text/html", html);
  });

  server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <title>Chevaux</title>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
            * {
                padding: 0;
                margin: 0;
                box-sizing: border-box;
            }

            html {
                font-family: Helvetica, serif;
                margin: 0 auto;
                text-align: center;
                height: 100%;
            }
            #content {
                transition: background-color 0.5s;
            }
            h1 {
                margin: 2em;
            }
            p {
                font-size: 30px;
                height: 100px;
            }
            div p {
                font-size: 20px;
                height: fit-content;
                margin: 10px 0;
            }
            div#redSide {
                display: grid;
                position: absolute;
                bottom: 50px;
                left: 50px;
            }
            div#greenSide {
                display: grid;
                position: absolute;
                bottom: 50px;
                right: 50px;
            }
            div#redSide div {
                height: 100px;
                width: 100px;
                background-color: red;
                border-radius: 50%;
                margin: auto;
            }
            div#greenSide div {
                height: 100px;
                width: 100px;
                background-color: green;
                border-radius: 50%;
                margin: auto;
            }
        </style>
    </head>
    <body id="content">
        <h1>Bonne chance !</h1>

        <p id="question">)" + questions[actualQuestionIndex].question + R"(</p>

        <div id="redSide">
            <p id="red">)" + questions[actualQuestionIndex].redDisplay + R"(</p>
            <div id="redCircle"></div>
        </div>

        <div id="greenSide">
            <p id="green">)" + questions[actualQuestionIndex].greenDisplay + R"(</p>
            <div id="greenCircle"></div>
        </div>

        <script>
            var websocket;
            var gateway = `ws://${window.location.hostname}/ws`;
            var paragraphQuestion = document.querySelector('#question');
            var paragraphRed = document.querySelector('#red');
            var paragraphGreen = document.querySelector('#green');

            function onOpen(event) {
                console.log('Connection opened');
            }
            function onClose(event) {
                console.log('Connection closed');
                setTimeout(initWebSocket, 2000);
            }
            function onMessage(event) {
                if (isJsonString(event.data)){
                    let dataJSON = JSON.parse(event.data);
                    console.log(dataJSON);
                    let backColor = "red";

                    if(dataJSON.isLastVictory == "") {
                        backColor = "white";
                    }
                    else {
                        if(dataJSON.isLastVictory == "1") backColor = "green";
                    }
                    
                    document.body.style.backgroundColor = backColor;
                    setTimeout(() => {document.body.style.backgroundColor = "white";}, 500);

                    paragraphQuestion.innerText = dataJSON.newQuestion;
                    paragraphGreen.innerText = dataJSON.newGreen;
                    paragraphRed.innerText = dataJSON.newRed;
                }
                else {
                    if (event.data === "RESET") window.location.href = '/';
                }
            }
            function initWebSocket() {
                console.log('Trying to open a WebSocket connection...');
                websocket = new WebSocket(gateway);
                websocket.onopen    = onOpen;
                websocket.onclose   = onClose;
                websocket.onmessage = onMessage; // <-- add this line
            }

            window.addEventListener('load', onLoad);
            function onLoad(event) {
                initWebSocket();
            }
            function isJsonString(str) {
              try {
                  JSON.parse(str);
              } catch (e) {
                  return false;
              }
              return true;
            }
        </script>
    </body>
    </html>)";

    isGameStarted = true;

    request->send(200, "text/html", html);
  });

  // INIT SERVERS -----------------------------------------------------------------------
  server.begin();
  initWebSocket();
}

void loop() {
  ws.cleanupClients();

  if (isGameStarted) {
    checkTouch();
    checkButtonsAndLimitSwitches();

    activeMotor();
  }

  checkTimeout();
}
