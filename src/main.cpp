// #include <Arduino.h>
#include "esp_camera.h"
#include "camera_index.h"
#include <pinClass.h>
#include <WiFi.h>
// #include <AsyncTCP.h>
// #include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include "esp_http_server.h"
#include "LittleFS.h"
// #include "socket.h"

#define PACKET_SIZE 512


#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
#include "camera_pins.h"

// AsyncWebServer server(80);
// AsyncWebSocket ws("/ws");
JSONVar readings;

const char* ssid = "androidogiro";
const char* password = "sfqt8592";

unsigned long lastTime = 0;
unsigned long timerDelay = 500;

camera_fb_t * fb = nullptr;

PinClass pin19, pin20, pin21, pin45, pin47, pin38;

#define PART_BOUNDARY "123456789000000000000987654321"

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

int numero_port; 

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

static httpd_ws_frame_t ws_pkt;
static uint8_t ws_buffer[128];


void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(ssid);
  Serial.println(WiFi.localIP());
}

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */


struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void *arg)
{
    static const char * data = "Async data";
    struct async_resp_arg *resp_arg = (struct async_resp_arg *)arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = (async_resp_arg *) malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) {
        return ESP_ERR_NO_MEM;
    }
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
    if (ret != ESP_OK) {
        free(resp_arg);
    }
    return ret;
}

// Fonction pour gérer les requêtes WebSocket (établissement de la connexion)
static esp_err_t websocket_connect_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        printf("Nouvelle connexion WebSocket\n");
        return ESP_OK;
    }
    return ESP_FAIL;
}

/*
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 */
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        Serial.printf("Handshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        Serial.printf("httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    Serial.printf("frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = (uint8_t*) calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            Serial.println("Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            Serial.printf("httpd_ws_recv_frame failed with %d \n", ret);
            free(buf);
            return ret;
        }
        Serial.printf("Got packet with message: %s \n", ws_pkt.payload);
        // if (strcmp((char*)ws_pkt.payload, "toggleBlueLight") == 0) {
        //   pin19.toggleLight();
        //   pin20.lightOnOff(false);
        //   pin21.digitalSpeed();
        // }

        String message = String((char *)ws_pkt.payload);
        JSONVar myObj = JSON.parse(message);  // Utilisation de Arduino_JSON pour parser

        if (JSON.typeof(myObj) == "undefined") {
          Serial.println("JSON parsing failed!");
          return 0;
        }

        if (myObj.hasOwnProperty("joystick")) {
          int joystickValue = int(myObj["joystick"]);
          String directionStr = (const char*) myObj["direction"];
          const char* direction = directionStr.c_str();
          Serial.println("Joystick value: " + String(joystickValue) + " direction: " + directionStr);
          if (strcmp(direction, "N") == 0) {
            Serial.println("we are on north");
            pin19.lightOnOff(true);
            pin20.lightOnOff(false);
            pin45.lightOnOff(true);
            pin47.lightOnOff(false);
          } else if (strcmp(direction, "S") == 0) {
            Serial.println("we are on south");
            pin19.lightOnOff(false);
            pin20.lightOnOff(true);
            pin45.lightOnOff(false);
            pin47.lightOnOff(true);
          } else if (strcmp(direction, "E") == 0) {
            Serial.println("we are on East");
            pin19.lightOnOff(false);
            pin20.lightOnOff(true);
            pin45.lightOnOff(true);
            pin47.lightOnOff(false);
          } else if (strcmp(direction, "O") == 0) {
            Serial.println("we are on West");
            pin19.lightOnOff(true);
            pin20.lightOnOff(false);
            pin45.lightOnOff(false);
            pin47.lightOnOff(true);
          } else {
            Serial.println("we are on nowhere");
            pin19.lightOnOff(true);
            pin20.lightOnOff(true);
            pin45.lightOnOff(true);
            pin47.lightOnOff(true);
          }
          pin21.digitalSpeed(abs(joystickValue));
          pin38.digitalSpeed(abs(joystickValue));
        }

        // if (doc.containsKey("joystick")) {
        //   int joystickValue = doc["joystick"];
        //   // Convertir et appliquer la commande au moteur
        //   Serial.println("Joystick: " + String(joystickValue));
        //   pin19.toggleLight();
        //   pin20.lightOnOff(false);
        //   pin21.digitalSpeed(joystickValue);
        // }
    }
    
    Serial.printf("Packet type: %d", ws_pkt.type);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char*)ws_pkt.payload,"Trigger async") == 0) {
        free(buf);
        return trigger_async_send(req->handle, req);
        return ret;
    }

    readings["incMessage"] = (const char*)ws_pkt.payload;
    String jsonString = JSON.stringify(readings);
    uint8_t* jsonPayload = (uint8_t*)jsonString.c_str();
    ws_pkt.payload = jsonPayload;
    ws_pkt.len = jsonString.length();
    ret = httpd_ws_send_frame(req, &ws_pkt);
    // ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
      Serial.printf("httpd_ws_send_frame failed with %d", ret);
    }
    free(buf);
    return ret;
}

// void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
//   AwsFrameInfo *info = (AwsFrameInfo*)arg;
//   if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
//     data[len] = 0;
//     String message = (char*)data;
//     // Check if the message is "getReadings"
//     if (strcmp((char*)data, "getReadings") == 0) {
//       //if it is, send current sensor readings
//       readings["pir"] = pin15.buttonPressed();
//       String sensorReadings = JSON.stringify(readings);
//       Serial.print(sensorReadings);
//       ws.textAll(sensorReadings);
//     }
//     if (strcmp((char*)data, "toggleBlueLight") == 0) {
//       pin19.toggleLight();
//       Serial.print("toggle blue light");
//     }
//   }
// }

// void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
//   switch (type) {
//     case WS_EVT_CONNECT:
//       Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
//       break;
//     case WS_EVT_DISCONNECT:
//       Serial.printf("WebSocket client #%u disconnected\n", client->id());
//       break;
//     case WS_EVT_DATA:
//       handleWebSocketMessage(arg, data, len);
//       break;
//     case WS_EVT_PONG:
//     case WS_EVT_ERROR:
//       break;
//   }
// }

// void initWebSocket() {
//   ws.onEvent(onEvent);
//   server.addHandler(&ws);
// }

void initPins() {
  // pin19.init(47, OUTPUT);
  // pin19.init(48, OUTPUT);
  // pin19.init(45, OUTPUT);
  //pin19.init(21, OUTPUT);
  // pin19.init(20, OUTPUT);
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW); 

  pin19.init(19, OUTPUT);
  pin20.init(20, OUTPUT);
  pin21.init(21, OUTPUT);
  // pin16.init(16, INPUT_PULLDOWN);
  // pin15.init(15, INPUT);
  pin45.init(45, OUTPUT);
  pin47.init(47, OUTPUT);
  pin38.init(38, OUTPUT);
}

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

// static esp_err_t index_handler(httpd_req_t *req) {
//   httpd_resp_set_type(req, "text/html");
//   httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
//   sensor_t *s = esp_camera_sensor_get();
//   if (s != NULL) {
//     if (s->id.PID == OV3660_PID) {
//       return httpd_resp_send(req, (const char *)index_ov3660_html_gz, index_ov3660_html_gz_len);
//     } else if (s->id.PID == OV5640_PID) {
//       return httpd_resp_send(req, (const char *)index_ov5640_html_gz, index_ov5640_html_gz_len);
//     } else {
//       return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
//     }
//   } else {
//     log_e("Camera sensor not found");
//     return httpd_resp_send_500(req);
//   }
// }

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
            while (file.available()) Serial.write(file.read());
            Serial.println();
        }
        file = root.openNextFile();
    }
}

// Handler pour servir le fichier index.html
esp_err_t index_handler(httpd_req_t *req) {
    // Ouvrir le fichier index.html dans LittleFS
    FILE* f = fopen("/littlefs/index.html", "r");
    if (f == NULL) {
        listDir(LittleFS, "/", 3);
        // Si le fichier ne peut pas être ouvert, renvoyer une erreur 404
        Serial.println("Fichier non lu");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Lire le contenu du fichier et envoyer les données en tant que réponse HTTP
    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {
        httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN);
    }

    // Fermer le fichier et terminer la réponse HTTP
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}


static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  struct timeval _timestamp;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[128];

  static int64_t last_frame = 0;
  if (!last_frame) {
    last_frame = esp_timer_get_time();
  }

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "60");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      log_e("Camera capture failed");
      res = ESP_FAIL;
    } else {
      _timestamp.tv_sec = fb->timestamp.tv_sec;
      _timestamp.tv_usec = fb->timestamp.tv_usec;
        if (fb->format != PIXFORMAT_JPEG) {
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if (!jpeg_converted) {
            log_e("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      log_e("Send frame failed");
      break;
    }
    int64_t fr_end = esp_timer_get_time();
    int64_t frame_time = fr_end - last_frame;
    frame_time /= 1000;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);
#endif
    log_i(
      "MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)"
      ,
      (uint32_t)(_jpg_buf_len), (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time, avg_frame_time, 1000.0 / avg_frame_time
    );
  }

  return res;
}

// ********************************************************
// web_handler: construction de la page web

static esp_err_t web_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "identity");
  sensor_t * s = esp_camera_sensor_get();

  char pageWeb[175] = "";
  strcat(pageWeb, "<!doctype html> <html> <head> <title id='title'>ESP32-CAM</title> </head> <body> <img id='stream' src='http://");
  // l'adresse du stream server (exemple: 192.168.0.145:81):
  char adresse[20] = "";
  sprintf (adresse, "%d.%d.%d.%d:%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3],numero_port);
  strcat(pageWeb, adresse);
  strcat(pageWeb, "/stream'> </body> </html>");
  int taillePage = strlen(pageWeb);

  return httpd_resp_send(req, (const char *)pageWeb, taillePage);
}

// Handler pour servir un fichier CSS
esp_err_t css_handler(httpd_req_t *req) {
    // Ouvrir le fichier CSS dans LittleFS
    FILE* f = fopen("/littlefs/style.css", "r");
    if (f == NULL) {
        // Si le fichier ne peut pas être ouvert, renvoyer une erreur 404
        Serial.println("Le fichier CSS n'a pas pu être ouvert.");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Définir l'en-tête de réponse HTTP pour indiquer le type de contenu
    httpd_resp_set_type(req, "text/css");

    // Lire le contenu du fichier et envoyer les données en tant que réponse HTTP
    char buf[256];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
        esp_err_t ret = httpd_resp_send_chunk(req, buf, read_bytes);
        if (ret != ESP_OK) {
            fclose(f);
            httpd_resp_send_chunk(req, NULL, 0); // Terminer la réponse en cas d'erreur
            Serial.println("Erreur lors de l'envoi du fichier CSS.");
            return ret;
        }
    }

    // Fermer le fichier et terminer la réponse HTTP
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // Terminer la réponse HTTP correctement
    Serial.println("Le fichier CSS a été envoyé avec succès.");
    return ESP_OK;
}


// Handler pour servir le fichier favicon.ico
esp_err_t favicon_handler(httpd_req_t *req) {
    Serial.println("traitement du favicon.ico");
    // Ouvrir le fichier favicon.ico dans LittleFS
    FILE* f = fopen("/littlefs/favicon.ico", "r");
    if (f == NULL) {
        // Si le fichier ne peut pas être ouvert, renvoyer une erreur 404
        Serial.println("Le fichier favicon.ico n'a pas pu être ouvert.");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Définir l'en-tête de réponse HTTP pour indiquer le type de contenu
    httpd_resp_set_type(req, "image/x-icon");

    // Lire le contenu du fichier et envoyer les données en tant que réponse HTTP
    char buf[256];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
        esp_err_t ret = httpd_resp_send_chunk(req, buf, read_bytes);
        if (ret != ESP_OK) {
            fclose(f);
            httpd_resp_send_chunk(req, NULL, 0); // Terminer la réponse en cas d'erreur
            Serial.println("Erreur lors de l'envoi du favicon.ico.");
            return ret;
        }
    }

    // Fermer le fichier et terminer la réponse HTTP
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // Terminer la réponse HTTP correctement
    Serial.println("Le fichier favicon.ico a été envoyé avec succès.");
    return ESP_OK;
}


// ********************************************************
// startCameraServer: démarrage du web server et du stream server

void startCameraServer() {
  httpd_config_t configHttpd = HTTPD_DEFAULT_CONFIG();

  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  httpd_uri_t favicon_uri = {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = favicon_handler,
    .user_ctx  = NULL
};

httpd_uri_t css_uri = {
    .uri       = "/style.css",
    .method    = HTTP_GET,
    .handler   = css_handler,
    .user_ctx  = NULL
};


  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
#ifdef CONFIG_HTTPD_WS_SUPPORT
    ,
    .is_websocket = true,
    .handle_ws_control_frames = false,
    .supported_subprotocol = NULL
#endif
  };

  // Configuration de l'URI pour le WebSocket
  httpd_uri_t ws_uri = {
      .uri       = "/ws",
      .method    = HTTP_GET,
      .handler   = ws_handler,
      .user_ctx  = NULL,
      .is_websocket = true
  };

  httpd_uri_t static_uri = {
    .uri       = "/*", // Matches all requests
    .method    = HTTP_GET,
    .handler   = [](httpd_req_t *req) {
        std::string filename = "/littlefs/";
        filename.append(req->uri + 1); // Skip the leading '/'
        Serial.println(filename.c_str());
        if (LittleFS.exists(req->uri)) {
            FILE* file = fopen(filename.c_str(), "r");
            Serial.println(req->uri + 1);
            if (strcmp(req->uri + 1, "style.css") == 0) {
              httpd_resp_set_type(req, "text/css");
            } else {
              httpd_resp_set_type(req, "text/html");
            }

            char line[256];
            while (fgets(line, sizeof(line), file) != NULL) {
                httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN);
            }

            fclose(file);
            httpd_resp_send_chunk(req, NULL, 0);
            return ESP_OK;
        }
        return ESP_FAIL;
    },
    .user_ctx  = NULL
};

  Serial.printf("Demarrage du web server sur le port: '%d'\n", configHttpd.server_port);
  configHttpd.uri_match_fn = httpd_uri_match_wildcard;
  if (httpd_start(&camera_httpd, &configHttpd) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &favicon_uri);
    // httpd_register_uri_handler(camera_httpd, &css_uri);
    httpd_register_uri_handler(camera_httpd, &ws_uri);
    httpd_register_uri_handler(camera_httpd, &static_uri);
  }

  configHttpd.server_port += 1;
  configHttpd.ctrl_port += 1;
  Serial.printf("Demarrage du stream server sur le port: '%d'\n", configHttpd.server_port);
  if (httpd_start(&stream_httpd, &configHttpd) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }


  numero_port = configHttpd.server_port;
}

void setup() {
  Serial.begin(115200);
  initPins();
  initWiFi();
  initLittleFS();
  // initWebSocket();


  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 40000000;
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 30;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
  #if CONFIG_IDF_TARGET_ESP32S3
      config.fb_count = 2;
  #endif
    }

  #if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
  #endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

  #if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
  #endif

  #if defined(CAMERA_MODEL_ESP32S3_EYE)
    s->set_vflip(s, 0);
  #endif

    // Web Server Root URL
  // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   request->send(LittleFS, "/index.html", "text/html");
  // });

  // server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request){
  //     handleStreamRequest(request);
  // });

  // server.begin();

  // s->set_brightness(s, 1);
  
  startCameraServer();
  

  Serial.print("La camera est prete.  Utilisez 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' pour vous connecter.");
}

bool lastState;

void loop() {
  // if ((millis() - lastTime) > timerDelay) {
  //   readings["pir"] = pin15.buttonPressed();
  //   if (lastState != pin15.buttonPressed()) {
  //     ws.textAll(JSON.stringify(readings));
  //     lastState = pin15.buttonPressed();
  //   }
  //   lastTime = millis();
  // }
}


