// IMPORTS ------------------------------------------------------------------------------
#include "WiFi.h"
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"

// SERVERS/WIFI -------------------------------------------------------------------------
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char *ssid = "MyAccessPoint"; // Don't forget to change on each ESP32.
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
const int touchThreshold = 20; //20 for the near touch, 2 for the far one.
int touchValue;
int lastTouchValue;
bool isTouching = false;

// MOTOR VARIABLES ----------------------------------------------------------------------
const int dirPin = 19;
const int stepPin = 18;
const int stepsPerRev = 95; //105 for the first track, 95 for the others.
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
int numQuestions;
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
  {"Numérisation est synonyme de transformation numérique.", "Vrai", "Faux", RED},
  {"La transformation numérique c’est remodeler son site web au goût du jour.", "Vrai", "Faux", RED},
  {"Quel est le pourcentage d’échec des projets numériques au sein des entreprises qui ne sont pas accompagnées ?", "80%", "67%", GREEN},
  {"Une mauvaise prise en compte de l’humain est souvent la principale cause des échecs de transformation numérique.", "Vrai", "Faux", GREEN},
  {"Quel est le pourcentage d’organisations qui prévoient des investissements technologiques ?", "56%", "76%", RED},
  {"La transformation numérique peut réduire jusqu’à 50% les coûts d’exploitation d’une entreprise.", "Vrai", "Faux", GREEN},
  {"Le manque de main-d’œuvre constitue le principal frein des PME à effectuer leur virage numérique.", "Vrai", "Faux", RED},
  {"Les entreprises paient en moyenne 150 000$ lors d’une cyberattaque de rançongiciel.", "Vrai", "Faux", GREEN},
  {"Le marketing par courriel peut rapporter jusqu’à 45$ par dollar investi.", "Vrai", "Faux", GREEN},
  {"Plus de la moitié des consommateur·rice·s n’envisagent pas acheter d’une entreprise sans site web.", "Vrai", "Faux", GREEN},
  {"Le terme mégadonnées (big data) existe depuis les années 1990.", "Vrai", "Faux", GREEN},
  {"L’automatisation des processus consiste à augmenter la charge de travail des employés.", "Vrai", "Faux", RED},
  {"Le terme d’intelligence artificielle fait son apparition en :", "1956", "1976", GREEN},
  {"Il y a plus de 20 milliards d’objets connectés dans le monde en 2023.", "Vrai", "Faux", GREEN},
  {"La manufacture intelligente signifie que de l'intelligence artificielle y est utilisée.", "Vrai", "Faux", RED},
  {"L’essentiel de la transformation numérique se concentre sur la technologie.", "Vrai", "Faux", RED},
  {"49% des entreprises n’ont pas une stratégie numérique bien établie.", "Vrai", "Faux", GREEN},
  {"La transformation numérique a un début et une fin.", "Vrai", "Faux", RED},
  {"Les dépenses mondiales consacrées à la transformation numérique atteindront ... en 2023.", "780B$", "2 300B$", RED},
  {"L’innovation et la transformation numérique vont toujours de pair.", "Vrai", "Faux", RED},
  {"Quel est le nombre de professionnels en Technologie de l’information au Québec en novembre 2020 ?", "156 700", "262 800", RED},
  {"Dans la dernière décennie, la croissance du stock de robots dans l’industrie manufacturière est de :", "80%", "160%", RED},
  {"Près de la moitié des entreprises font du marketing numérique sans avoir de stratégie claire et définie.", "Vrai", "Faux", GREEN},
  {"La transformation numérique est une innovation de rupture.", "Vrai", "Faux", RED},
};

// GAME VARIABLES -----------------------------------------------------------------------
bool isGameStarted = false;
bool isGameFinished = false;

// OTHERS VARIABLES ---------------------------------------------------------------------
bool isMoving = false;
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
        if (isGameStarted) {
          Color color;

          if (btnPin == btnGreenPin) color = GREEN;
          if (btnPin == btnRedPin) color = RED;

          if (questions[actualQuestionIndex].answer == color) isVictory = true;
          else isVictory = false;

          actualQuestionIndex++;

          String jsonStr = "";
          if (actualQuestionIndex - 1 >= numQuestions - 1) {
            jsonStr = generateJSONString(isVictory, "", "", "");
          }
          else {
            jsonStr = generateJSONString(
              isVictory,
              questions[actualQuestionIndex].question,
              questions[actualQuestionIndex].greenDisplay,
              questions[actualQuestionIndex].redDisplay
            );
          }
          
          notifyClients(jsonStr);
          
          if (isVictory) {
            isMoving = true;
            digitalWrite(enablePin, LOW);
            delay(500);
          }
          else {
            delay(2000);
          }

          if (actualQuestionIndex - 1 >= numQuestions - 1) {
            isGameFinished = true;
            isGameStarted = false;
            actualQuestionIndex = 0;
            notifyClients("LOSE");
          }
        }
        else if (isGameFinished){
          isGameFinished = false;
          isGameStarted = false;
          notifyClients("PASS");
          returnBeginning();
        }
        else {
          isGameFinished = false;
          isGameStarted = true;
          notifyClients("PASS");
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

        if (limitPin == limitRightPin) {
          isGameFinished = true;
          isGameStarted = false;
          actualQuestionIndex = 0;
          notifyClients("WIN");
        }
      }
    }
  }
  lastLimitState = newLimitState;
}

void checkButtonsAndLimitSwitches() {
  //It's important to keep in this order
  checkLimitSwitch(limitRightState, lastLimitRightState, lastDebounceTimeLimitRight, limitRightPin);
  checkLimitSwitch(limitLeftState, lastLimitLeftState, lastDebounceTimeLimitLeft, limitLeftPin);

  checkButton(btnGreenState, lastBtnGreenState, lastDebounceTimeBtnGreen, btnGreenPin);
  checkButton(btnRedState, lastBtnRedState, lastDebounceTimeBtnRed, btnRedPin);
}

void activeMotor() {
  if (isMoving) {
    if (btnGreenState == HIGH) {
      moveForward(1);
    }
    if (btnRedState == HIGH) {
      moveForward(1);
    }
    if (limitRightState == HIGH) {
      moveBackward(0.5);
    }
    if (limitLeftState == HIGH) {
      moveForward(0.5);
    }
    if (isTouching) {
      returnBeginning();
      isTouching = false;
    }

    isMoving = false;
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

void moveBackward(float times) {
  digitalWrite(dirPin, HIGH);
  for (int i = 0; i < stepsPerRev * times; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(3000);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(3000);

    if (!digitalRead(limitLeftPin) == HIGH) {
      moveForward(0.5);
      break;
    }
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

void checkTouch() {
  int newTouchValue = touchRead(touchPin);

  if (newTouchValue != lastTouchValue) {
    lastDebounceTimeTouch = millis();
  }

  if ((millis() - lastDebounceTimeTouch) > debounceDelay) {
    if (newTouchValue != touchValue) {
      touchValue = newTouchValue;

      if(touchValue <= touchThreshold) {
        isMoving = true;
        isTouching = true;
        
        isGameFinished = false;
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

  numQuestions = sizeof(questions) / sizeof(questions[0]);

  // INIT WIFI/AP -----------------------------------------------------------------------
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("Adresse IP du point d'accès : ");
  Serial.println(WiFi.softAPIP());

  // WEB SERVER ROUTES ------------------------------------------------------------------
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"(
    <!DOCTYPE html>
    <html lang='fr'>
    <head>
        <title>COlab</title>
        <meta charset='UTF-8'>
        <meta name='viewport' content='width=device-width, initial-scale=1'>
        <style>
            * {
                padding: 0;
                margin: 0;
                box-sizing: border-box;
            }

            html,
            body {
                font-family: Helvetica, serif;
                text-align: center;
                height: 100%;
                width: 100%;
                color: #FFF;
                background-image: linear-gradient(to bottom right, #0067C6, #8370EC, #8370EC);
            }

            h1 {
                font-size: 3.5em;
                padding: 1em;
                white-space: nowrap;
                color: #142741;
            }

            div {
                padding: 1em;
                font-size: 25px;
            }

            div#greenCircle {
                display: inline-block;
                height: 50px;
                width: 50px;
                background-color: #080;
                border-radius: 50%;
                margin: 0 0.5em;
                vertical-align: middle;
                border: #000 2px solid;
            }
        </style>
    </head>
    <body>
    <h1>Bienvenue !</h1>

    <div>Attendez que le départ soit donné par l'animateur et appuyez sur
        <div id='greenCircle'></div>
        pour débuter la course !
    </div>

    <script>
        // WS
        var websocket;
        var gateway = `ws://${window.location.hostname}/ws`;

        window.addEventListener('load', onLoad);

        // WS FUNCTIONS
        function onLoad(event) {
            initWebSocket();
        }

        function initWebSocket() {
            console.log('Trying to open a WebSocket connection...');
            websocket = new WebSocket(gateway);
            websocket.onopen = onOpen;
            websocket.onclose = onClose;
            websocket.onmessage = onMessage;
        }

        function onOpen(event) {
            console.log('Connection opened');
        }

        function onClose(event) {
            console.log('Connection closed');
            setTimeout(initWebSocket, 2000);
        }

        function onMessage(event) {
            if (event.data === 'PASS') window.location.href = '/start';
        }
    </script>
    </body>
    </html>)";

    request->send(200, "text/html", html);
  });

  server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"(
    <!DOCTYPE html>
    <html lang='fr'>
    <head>
        <title>COlab</title>
        <meta charset='UTF-8'>
        <meta name='viewport' content='width=device-width, initial-scale=1'>
        <style>
            * {
                padding: 0;
                margin: 0;
                box-sizing: border-box;
            }

            html,
            body {
                font-family: Helvetica, serif;
                text-align: center;
                height: 100%;
                width: 100%;
                color: #FFF;
                background-image: linear-gradient(to bottom right, #0067C6, #8370EC, #8370EC);
            }

            p {
                font-size: 40px;
                height: 100px;
                padding: 1em;
            }

            div#filter {
                position: absolute;
                top: 0;
                z-index: 1;
                width: 100%;
                height: 100%;
                opacity: 0;
            }

            div#filter p {
                position: absolute;
                top: 50%;
                left: 50%;
                z-index: 2;
                padding: 0;
                width: fit-content;
                height: fit-content;
                font-size: 14em;
                transform: translate(-50%, -50%);
            }

            div#chrono {
                font-size: 5vh;
                margin: 1em;
                text-align: end;
                font-weight: bold;
                color: #77E6B0;
            }

            p#question {
                height: fit-content;
                font-size: 6vh;
            }

            div#content {
                display: grid;
                grid-template-rows: auto auto auto;
            }

            div#grid {
                display: grid;
                grid-template-columns: 50% 50%;

                width: 80%;
                height: fit-content;
                min-height: 20%;
                margin: auto;
            }

            p#redOption,
            p#greenOption {
                font-size: 5vh;
                height: fit-content;
                margin: 0.5em 0;
                padding: 0.5em;
                text-align: center;
                color: #FFF;
            }

            div#redCircle,
            div#greenCircle {
                height: 80px;
                width: 80px;
                background-color: #F00;
                border: #000 2px solid;
                border-radius: 50%;
                margin: 1em auto;
            }

            div#greenCircle {
                background-color: #080;
            }
        </style>
    </head>
    <body>
    <div id='filter'>
        <p id='iconPara'></p>
    </div>

    <div id='content'>
        <div id='chrono'>00m 00s 000ms</div>

        <p id='question'>)" + questions[actualQuestionIndex].question + R"(</p>

        <div id='grid'>
            <p id='redOption'>)" + questions[actualQuestionIndex].redDisplay + R"(</p>
            <p id='greenOption'>)" + questions[actualQuestionIndex].greenDisplay + R"(</p>
            <div id='redCircle'></div>
            <div id='greenCircle'></div>
        </div>
    </div>

    <script>
        // WS
        let websocket;
        let gateway = `ws://${window.location.hostname}/ws`;

        // HTML ELEMENTS
        let filter = document.querySelector('#filter');
        let iconPara = document.querySelector('#iconPara');
        let paragraphQuestion = document.querySelector('#question');
        let paragraphRed = document.querySelector('#redOption');
        let paragraphGreen = document.querySelector('#greenOption');
        let icons = ['✔', '✖'];

        // CHRONOS
        let startTime;
        let intervalId;

        window.addEventListener('load', onLoad);
        startChrono();

        // WS FUNCTIONS
        function onLoad(event) {
            initWebSocket();
        }

        function initWebSocket() {
            console.log('Trying to open a WebSocket connection...');
            websocket = new WebSocket(gateway);
            websocket.onopen = onOpen;
            websocket.onclose = onClose;
            websocket.onmessage = onMessage;
        }

        function onOpen(event) {
            console.log('Connection opened');
        }

        function onClose(event) {
            console.log('Connection closed');
            setTimeout(initWebSocket, 2000);
        }

        function onMessage(event) {
            if (isJsonString(event.data)) {
                let dataJSON = JSON.parse(event.data);

                let filterOpacity = '1';
                let filterColor = '#FF5050';
                let iconColor = '#F00';
                let delay = 2000;
                iconPara.innerText = icons[1];

                if (dataJSON.isLastVictory == '') {
                    filterOpacity = '0';
                    filterColor = '';
                    iconColor = '';
                    iconPara.innerText = '';
                } else {
                    if (dataJSON.isLastVictory == '1') {
                        filterOpacity = '1';
                        filterColor = '#50FF50';
                        iconColor = '#0F0';
                        delay = 500;
                        iconPara.innerText = icons[0];
                    }
                }

                filter.style.opacity = filterOpacity;
                filter.style.backgroundColor = filterColor;
                iconPara.style.color = iconColor;

                setTimeout(() => {
                    filter.style.opacity = '0';
                    filter.style.backgroundColor = '';
                    iconPara.style.color = '';
                }, delay);

                paragraphQuestion.innerText = dataJSON.newQuestion;
                paragraphGreen.innerText = dataJSON.newGreen;
                paragraphRed.innerText = dataJSON.newRed;
            } else {
                if (event.data === 'RESET') {
                    document.cookie = '';
                    window.location.href = '/';
                } else if (event.data === 'WIN') {
                    endGame(true);
                } else if (event.data === 'LOSE') {
                    endGame(false);
                }
            }
        }

        // CHRONOS FUNCTIONS
        function startChrono() {
            startTime = Date.now();
            intervalId = setInterval(updateChrono, 1);
        }

        function updateChrono() {
            const currentTime = Date.now() - startTime;
            const minutes = Math.floor(currentTime / 60000);
            const seconds = Math.floor((currentTime % 60000) / 1000);
            const milliseconds = currentTime % 1000;

            const formattedTime = `${String(minutes).padStart(2, '0')}m ${String(seconds).padStart(2, '0')}s ${String(milliseconds).padStart(3, '0')}ms`;
            document.getElementById('chrono').textContent = formattedTime;
            document.cookie = 'chrono=' + formattedTime;
        }

        // OTHERS FUNCTIONS
        function isJsonString(str) {
            try {
                JSON.parse(str);
            } catch (e) {
                return false;
            }
            return true;
        }

        function endGame(isVictory) {
            document.cookie = 'isvictory=' + isVictory;
            window.location.href = '/end';
        }
    </script>
    </body>
    </html>)";

    isGameFinished = false;
    isGameStarted = true;

    request->send(200, "text/html", html);
  });

  server.on("/end", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"(
    <!DOCTYPE html>
    <html lang='fr'>
    <head>
        <title>COlab</title>
        <meta charset='UTF-8'>
        <meta name='viewport' content='width=device-width, initial-scale=1'>
        <style>
            * {
                padding: 0;
                margin: 0;
                box-sizing: border-box;
            }

            html,
            body {
                font-family: Avenir-Medium, sans-serif;
                text-align: center;
                height: 100%;
                max-height: 100vh;
                width: 100%;
                color: #FFF;
            }

            div#content {
                display: grid;
                grid-template-rows: auto auto auto auto;
            }

            p {
                font-size: 25px;
            }

            h1 {
                font-size: 12vh;
                padding: 1em 1em 0;
                white-space: nowrap;
                color: #142741;
            }

            div#chrono {
                padding: 0 2vh 2vh;
                font-size: 6vh;
                font-weight: bold;
                color: #77E6B0;
            }

            p#message {
                padding: 1em;
                font-size: 5vh;
            }

            div#clickText {
                display: flex;
                align-items: center;
                width: fit-content;

                margin-left: auto;
                margin-right: 10px;
            }

            div#redCircle {
                height: 50px;
                width: 50px;
                background-color: #F00;
                border-radius: 50%;
                border: #000 2px solid;
                margin: 0 1em;
            }
        </style>
    </head>
    <body>
    <div id='content'>
        <h1 id='title'>%TITLE%</h1>
        <div id='chrono'>%TIME%</div>
        <p id='message'>%MESSAGE%</p>

        <div id='clickText'>
            <p>Cliquez sur</p>
            <div id='redCircle'></div>
            <p>pour recommencer</p>
        </div>
    </div>

    <script>
        // WS
        var websocket;
        var gateway = `ws://${window.location.hostname}/ws`;

        // HTML ELEMENTS
        let title = document.querySelector('#title');
        let message = document.querySelector('#message');
        let chrono = document.querySelector('#chrono');

        window.addEventListener('load', onLoad);

        const timeCookie = getCookie('chrono');
        if (timeCookie !== null) {
            chrono.innerText = timeCookie;
        } else {
            window.location.href = '/';
        }

        const isVictoryCookie = getCookie('isvictory');
        if (isVictoryCookie !== null) {
            const isVictory = (/true/i).test(isVictoryCookie);
            if (isVictory) victory();
            else if (!isVictory) lose();
        } else {
            window.location.href = '/';
        }

        // WS FUNCTIONS
        function onLoad(event) {
            initWebSocket();
        }

        function initWebSocket() {
            console.log('Trying to open a WebSocket connection...');
            websocket = new WebSocket(gateway);
            websocket.onopen = onOpen;
            websocket.onclose = onClose;
            websocket.onmessage = onMessage;
        }

        function onOpen(event) {
            console.log('Connection opened');
        }

        function onClose(event) {
            console.log('Connection closed');
            setTimeout(initWebSocket, 2000);
        }

        function onMessage(event) {
            if (event.data === 'PASS') {
                document.cookie = '';
                window.location.href = '/';
            } else if (event.data === 'WIN') victory();
        }

        function victory() {
            title.innerText = 'Félicitations !';
            message.innerText = 'Vous avez atteint la fin de la course ! Pour parfaire vos connaissances en matière de transformation numérique, scannez le code QR situé sur les stations de jeu ! Merci d’avoir joué !';
            document.body.style.background = 'linear-gradient(to bottom right, #D0C3CE, #88A2EB)';
        }

        function lose() {
            title.innerText = 'Dommage…';
            message.innerText = 'Vous n’avez pas atteint la fin de la course. Nous vous recommandons de parfaire vos connaissances en matière de transformation numérique en scannant le code QR situé sur les stations de jeu ! Merci d’avoir joué !';
            document.body.style.background = 'linear-gradient(to bottom right, #8370EC, #88A2EB)';
        }

        function getCookie(name) {
            const cookies = document.cookie.split('; ');
            for (let i = 0; i < cookies.length; i++) {
                const cookie = cookies[i].split('=');
                if (cookie[0] === name) {
                    return cookie[1];
                }
            }
            return null;
        }
    </script>
    </body>
    </html>)";

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
  if (isGameFinished) {
    //Here we check also for the limit switch right because the player can empty his stock of questions but can also touch the right and win.

    //It's important to keep in this order
    checkLimitSwitch(limitRightState, lastLimitRightState, lastDebounceTimeLimitRight, limitRightPin);
    checkButton(btnRedState, lastBtnRedState, lastDebounceTimeBtnRed, btnRedPin);

    activeMotor();
  }
  else {
    checkButton(btnGreenState, lastBtnGreenState, lastDebounceTimeBtnGreen, btnGreenPin);
  }
}
