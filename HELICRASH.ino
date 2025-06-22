#include <EEPROM.h>
#include <LiquidCrystal.h>


// Clase para recorrer una secuencia de fotogramas de animación
class Animation {
  private:
    int _current_frame;
    int _frame_count;
    int *_frames;
  public:
    Animation(int frame_count, int *frames) {
      _current_frame = 0;
      _frame_count = frame_count;
      _frames = frames;
    }
    int next() {
      _current_frame++;
      if (_current_frame == _frame_count) {
        _current_frame = 0;
      }
      return _frames[_current_frame];
    }
    int prev() {
      _current_frame++;
      if (_current_frame < 0) {
        _current_frame += _frame_count;
      }
      return _frames[_current_frame];
    }
    int current() {
      return _frames[_current_frame];
    }
};



// Constantes para identificar cada sprite
const byte SPRITE_TAIL0 = 0;
const byte SPRITE_TAIL1 = 1;
const byte SPRITE_BODY0 = 2;
const byte SPRITE_BODY1 = 3;
const byte SPRITE_WALLB = 5;
const byte SPRITE_WALLT = 6;
const byte SPRITE_EXPL = 7;

// Notas para una melodía simple 
int melody[] = { 262, 294, 330, 349, 392, 440, 494, 523 };  // DO RE MI FA SOL LA SI DO
int melody_length = sizeof(melody) / sizeof(int);
int melody_index = 0;
unsigned long next_note_time = 0;



// Variables para manejo de color y animación
unsigned long led_next_update = 0;
int color_phase = 0; // Para animaciones suaves


// Datos de caracteres personalizados para los sprites
byte sprite_tail0[8] = {0b00000, 0b00000, 0b00000, 0b10001, 0b01111, 0b00101, 0b00000, 0b00001};
byte sprite_tail1[8] = {0b00000, 0b00000, 0b00000, 0b00101, 0b01111, 0b10001, 0b00000, 0b00001};
byte sprite_body0[8] = {0b00000, 0b00100, 0b00100, 0b11110, 0b10101, 0b11110, 0b10100, 0b11111};
byte sprite_body1[8] = {0b00000, 0b11111, 0b00100, 0b11110, 0b10101, 0b11110, 0b10100, 0b11111};
byte sprite_wallt[8] = {0b00000, 0b00100, 0b01001, 0b11110, 0b01001, 0b00100, 0b00000, 0b00000};
byte sprite_wallb[8] = {0b00000, 0b00100, 0b00100, 0b01010, 0b01110, 0b01010, 0b01110, 0b01010};
byte sprite_expl[8] = {0b00100, 0b10100, 0b01101, 0b01010, 0b01010, 0b10110, 0b00101, 0b00100};



// Secuencias de animación
int seq_tail[] = {SPRITE_TAIL0, SPRITE_TAIL1};
int seq_body[] = {SPRITE_BODY0, SPRITE_BODY1};
Animation anim_tail(2, seq_tail);
Animation anim_body(2, seq_body);


// Configuración del LCD
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);



// Constante del pin del parlante
const int speaker = 3;


// Algunas constantes del juego que se pueden ajustar para controlar la dificultad
const unsigned long frame_rate = 125;
unsigned long frame_next = 0;
int game_mode = 0;
unsigned long score = 0;
boolean new_highscore = false;
boolean first = true;
boolean button_state = false;
const unsigned debounce_time = 20;

boolean wait_for_button_release = false;

// Pines para los LEDs
const int LED_YELLOW1 = 10;
const int LED_YELLOW2 = 11;
const int LED_RED1    = 12;
const int LED_RED2    = 13;


volatile unsigned long next_read = 0;


// Generación "inteligente" de muros para que el juego no sea imposible
unsigned int mask_wall = 0b1111000000000000;


// Máscara de detección de colisiones del helicóptero
unsigned int mask_heli = 0b0000000000000011;


// Representaciones binarias de la ubicación de obstáculos
unsigned int walls_top = 0x0000;
unsigned int walls_bot = 0x0000;


// Velocidades máximas y mínimas de avance de los muros
unsigned long max_wall_advance_rate = 350;
unsigned long min_wall_advance_rate = 140;
unsigned long wall_advance_rate = max_wall_advance_rate;
unsigned long wall_advance_next = 0;


// Tiempos de animación y destello de muerte
unsigned long death_rate = 150;
unsigned long death_hold = 1500;

void setup() {
  pinMode(LED_YELLOW1, OUTPUT);
  pinMode(LED_YELLOW2, OUTPUT);
  pinMode(LED_RED1, OUTPUT);
  pinMode(LED_RED2, OUTPUT);

// Apagar los LEDs rojos al inicio
digitalWrite(LED_RED1, LOW);
digitalWrite(LED_RED2, LOW);

  pinMode(speaker,OUTPUT);
  read_button();
  // Semilla del generador de números aleatorios usando ruido del pin analógico 0
  unsigned long seed = 0;
  for (int n = 0; n < 32; ++n) {
    randomSeed(seed);
    delay(random() & 0xf);
    seed <<= 1;
    seed |= analogRead(1) & 1;
  }
  // Generar los caracteres personalizados para los sprites del juego
  lcd.createChar(SPRITE_TAIL0, sprite_tail0);
  lcd.createChar(SPRITE_TAIL1, sprite_tail1);
  lcd.createChar(SPRITE_BODY0, sprite_body0);
  lcd.createChar(SPRITE_BODY1, sprite_body1);
  lcd.createChar(SPRITE_WALLB, sprite_wallb);
  lcd.createChar(SPRITE_WALLT, sprite_wallt);
  lcd.createChar(SPRITE_EXPL, sprite_expl);

  // Iniciar la pantalla LCD
  lcd.begin(16, 2);
  lcd.noCursor();
  lcd.clear();
  delay(5);
  lcd.home();

  // Iniciar en la pantalla principal
  set_game_mode(0);


}

void loop() {
  boolean update = false;

  // Actualizar secuencias de animación
  unsigned long now = millis();
  if (frame_next < now) {
    anim_tail.next();
    anim_body.next();
    frame_next = now + frame_rate;
    update = true;
  }
  read_button();
  updateYellowLED();

  
  // Ejecutar acción según el modo de juego actual
  switch (game_mode) {
    case 0: game_home(update) ; break;
    case 1: game_play(update) ; break;
    case 2: game_over(update); break;
    default: game_mode = 0;
  }
  playBackgroundMusic(); 

}

// Estado de inicio
void game_home(boolean update) {
  static unsigned long lastBlink = 0;
  static bool showStart = true;
  static bool showingRecord = true;  // ← para controlar el estado de "mostrar récord"

  if (first) {
    first = false;
    score = get_highscore();
    lcd.clear();
    delay(5);
    wait_for_button_release = true;
    showingRecord = true;
  }

  read_button();

  // Paso 1: Mostrar récord hasta que se presione el botón
  if (showingRecord) {
    if (!button_state) {
      wait_for_button_release = false;  // esperar que suelte
    } else if (!wait_for_button_release && button_state) {
      showingRecord = false;
      wait_for_button_release = true;
      lcd.clear();
      delay(5);  // limpiar para mostrar la siguiente etapa
    }

    if (update) {
      lcd.setCursor(0, 0);
      lcd.print("HeliCrash ");
      lcd.write(anim_tail.current());
      lcd.write(anim_body.current());

      lcd.setCursor(0, 1);
      lcd.print("Record: ");
      lcd.print(score);
    }
    return;  // salir aquí si aún estamos mostrando el récord
  }

  // Paso 2: Parpadea "START para jugar"
  if (!button_state) {
    wait_for_button_release = false;
  } else if (!wait_for_button_release && button_state) {
    set_game_mode(1);  // iniciar juego
    return;
  }

  // Control del parpadeo
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    showStart = !showStart;
  }

  if (update) {
    lcd.setCursor(0, 0);
    lcd.print("HeliCrash ");
    lcd.write(anim_tail.current());
    lcd.write(anim_body.current());

    lcd.setCursor(0, 1);
    if (showStart) {
      lcd.print("START para jugar");
    } else {
      lcd.print("                ");  // limpiar línea para parpadeo
    }
  }

  updateYellowLED(); 
}



// Estado de juego
void game_play(boolean update) {
  if (first) {
    first = false;
    score = 0;
    new_highscore = false;
    walls_bot = 0;
    walls_top = 0;
    wall_advance_rate = max_wall_advance_rate;
    lcd.clear();
    delay(5);
  }

  unsigned long now = millis();

  if (now > wall_advance_next) {
    if (wall_advance_rate > min_wall_advance_rate) {
      wall_advance_rate--;
    }
    wall_advance_next = now + wall_advance_rate;

    walls_top >>= 1;
    walls_bot >>= 1;
    ++score;

    // Generación aleatoria de obstáculos
    if (((walls_top | walls_bot) & mask_wall) == 0) {
      if (random() & 1) {
        walls_top |= 0x8000;
      } else {
        walls_bot |= 0x8000;
      }
    }

    update = true;
  }

  if (update) {
    playBackgroundMusic();

    // Dibuja los muros
    for (int n = 0; n < 16; ++n) {
      lcd.setCursor(n, 0);
      lcd.write((walls_top & (1 << n)) ? SPRITE_WALLT : ' ');

      lcd.setCursor(n, 1);
      lcd.write((walls_bot & (1 << n)) ? SPRITE_WALLB : ' ');
    }

    // Verifica colisión
    bool colision = button_state
      ? (mask_heli & walls_bot)
      : (mask_heli & walls_top);

    // Mueve el helicóptero
    lcd.setCursor(0, button_state ? 1 : 0);
    if (colision) {
      noTone(speaker);  // Detener la música
      lcd.write(SPRITE_EXPL);
      lcd.write(SPRITE_EXPL);
      tone(speaker, 329, 100);  // Sonido de explosión
      delay(120);
      playDefeatSound();

      // Efecto de muerte 
      unsigned long death_stop = millis() + death_hold;
      digitalWrite(LED_RED1, HIGH);
      digitalWrite(LED_RED2, HIGH);
      while (millis() < death_stop) {
        // Delay simple
      }

      if (score > get_highscore()) {
        set_highscore(score);
        new_highscore = true;
      }

      set_game_mode(2);
    } else {
      lcd.write(anim_tail.current());
      lcd.write(anim_body.current());
    }
  }
}


// Estado de fin del juego
// Muestra la puntuación del jugador
void game_over(boolean update) {
  noTone(speaker);

  digitalWrite(LED_RED1, LOW);
  digitalWrite(LED_RED2, LOW);

  if (first) {
    first = false;
    lcd.clear(); 
    delay(5);
  }

  read_button();

  if (wait_for_button_release) {
    if (button_state) {
      wait_for_button_release = false;
    }
  } else {
    if (!button_state) {
      set_game_mode(0);
      return;
    }
  }

  if (update) {
    lcd.setCursor(0, 0);
    lcd.print(new_highscore ? "Nuevo Record!" : "## GAME OVER ##");
    lcd.setCursor(0, 1);
    lcd.print("Puntaje: ");
    lcd.print(score);
  }
}


// Cambia el modo de juego y reinicia algo del estado
void set_game_mode(int mode) {
  button_state = true;
  game_mode = mode;
  first = true;
  digitalWrite(LED_RED1, LOW);
  digitalWrite(LED_RED2, LOW);

  wait_for_button_release = true;  // ← nueva línea
  lcd.clear();
  
  delay(5);
  init_custom_chars();
}


// Recupera la puntuación más alta desde la EEPROM
unsigned long get_highscore() {
  unsigned long b0 = EEPROM.read(0);
  unsigned long b1 = EEPROM.read(1);
  unsigned long b2 = EEPROM.read(2);
  unsigned long b3 = EEPROM.read(3);
  unsigned long cs = EEPROM.read(4);
  if (((b0 + b1 + b2 + b3) & 0xff) == cs) {
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
  }
  return 0;
}

// Guarda la nueva puntuación más alta en EEPROM
void set_highscore(unsigned long score) {
  byte b0 = (byte)((score >> 0) & 0xff);
  byte b1 = (byte)((score >> 8) & 0xff);
  byte b2 = (byte)((score >> 16) & 0xff);
  byte b3 = (byte)((score >> 24) & 0xff);
  byte cs = b0 + b1 + b2 + b3;
  EEPROM.write(0, b0);
  EEPROM.write(1, b1);
  EEPROM.write(2, b2);
  EEPROM.write(3, b3);
  EEPROM.write(4, cs);
}

void read_button() {
  if (millis() > next_read) {
    next_read = millis() + 200;
    button_state = (analogRead(A0) > 1000);
  }
}








// Parpadeo constante de LEDs amarillos
void updateYellowLED() {
  static unsigned long lastToggle = 0;
  static bool state = false;
  if (millis() - lastToggle > 500) {
    lastToggle = millis();
    state = !state;
    digitalWrite(LED_YELLOW1, state);
    digitalWrite(LED_YELLOW2, state);
  }
}


void playBackgroundMusic() {
  unsigned long now = millis();
  unsigned long music_delay = map(wall_advance_rate, max_wall_advance_rate, min_wall_advance_rate, 600, 150);

  if (now >= next_note_time) {
    noTone(speaker);  // Asegura que el canal se libere antes de iniciar otro tono
    tone(speaker, melody[melody_index]);
    next_note_time = now + music_delay;
    melody_index = (melody_index + 1) % melody_length;
  }
}



void playDefeatSound() {
  int defeat_melody[] = { 523, 494, 440, 392, 349, 330, 294, 262 };
  for (int i = 0; i < 8; i++) {
    tone(speaker, defeat_melody[i], 80);
    delay(80);
  }
  noTone(speaker); 
}


void init_custom_chars() {
  lcd.createChar(SPRITE_TAIL0, sprite_tail0);
  lcd.createChar(SPRITE_TAIL1, sprite_tail1);
  lcd.createChar(SPRITE_BODY0, sprite_body0);
  lcd.createChar(SPRITE_BODY1, sprite_body1);
  lcd.createChar(SPRITE_WALLB, sprite_wallb);
  lcd.createChar(SPRITE_WALLT, sprite_wallt);
  lcd.createChar(SPRITE_EXPL, sprite_expl);
}
