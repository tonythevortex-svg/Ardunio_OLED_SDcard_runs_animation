#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <SD.h>
#include <esp_system.h>
#include <cstring>

// ============================================================
// ESP32-S3 VIEWER
// ST7789 320x240 landscape
//
// TFT
// MOSI  GPIO12
// SCK   GPIO11
// CS    GPIO10
// DC    GPIO46
// RST   GPIO8
//
// SD
// MOSI  GPIO12
// SCK   GPIO11
// MISO  GPIO13
// CS    GPIO41
//
// RAW FRAMES
// /1000.raw -> /1288.raw
//
// IMPORTANT:
// RAW files are LITTLE-ENDIAN RGB565.
// DO NOT BYTE SWAP.
// ============================================================


// ============================================================
// PINS
// ============================================================

#define SPI_SCLK 11
#define SPI_MOSI 12
#define SPI_MISO 13

#define TFT_CS   10
#define TFT_DC   46
#define TFT_RST   8

#define SD_CS    41


// ============================================================
// SCREEN
// ============================================================

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

constexpr size_t FRAME_BYTES =
  SCREEN_W * SCREEN_H * 2;


// ============================================================
// DISPLAY
// ============================================================

Adafruit_ST7789 tft =
  Adafruit_ST7789(
    &SPI,
    TFT_CS,
    TFT_DC,
    TFT_RST
  );

GFXcanvas16 *canvas = nullptr;


// ============================================================
// SPI SPEEDS
// ============================================================

constexpr uint32_t TFT_SPI_HZ = 40000000;
constexpr uint32_t SD_SPI_HZ  = 30000000;


// ============================================================
// EYE FILM
// ============================================================

constexpr int FIRST_FRAME = 1000;
constexpr int LAST_FRAME  = 1288;

constexpr int FRAME_COUNT =
  LAST_FRAME - FIRST_FRAME + 1;

// approx 10fps
constexpr uint32_t FRAME_TIME_MS = 100;


// ============================================================
// NORMAL EYE SEQUENCES
//
// RANDOM BETWEEN 3 AND 10 SECONDS
// ============================================================

constexpr int EYE_MIN_SECONDS = 3;
constexpr int EYE_MAX_SECONDS = 10;


// ============================================================
// EYE SIGNAL INTRUSIONS
//
// Occasionally interrupts TEXT.
// ============================================================

// 32 = roughly 32% of text pages may get an intrusion.
//
// Try:
// 20 = quite rare
// 30 = occasional
// 45 = noticeable
// 60 = quite disruptive

constexpr int EYE_INTRUSION_PERCENT = 32;


// duration of intrusion

constexpr uint32_t EYE_INTRUSION_MS = 1000;


// Don't start two eye sections from almost the same
// part of the film.

constexpr int MIN_EYE_START_SEPARATION = 40;

int lastEyeStart = -1;


// ============================================================
// FINAL EYE LOOP
// ============================================================

constexpr uint32_t FINAL_EYE_LOOP_MS = 30000;


// ============================================================
// TEXT STYLE
// ============================================================

constexpr int TEXT_SIZE = 2;


// ============================================================
// SAFE BORDER / MARGIN
//
// Increase these for more empty space around text.
// ============================================================

constexpr int TEXT_MARGIN_X = 18;
constexpr int TEXT_MARGIN_Y = 18;


constexpr int CHAR_W =
  6 * TEXT_SIZE;

constexpr int CHAR_H =
  8 * TEXT_SIZE;

constexpr int LINE_H = 18;


constexpr int MAX_COLS =

  (SCREEN_W -
   (TEXT_MARGIN_X * 2))

  / CHAR_W;


constexpr int MAX_ROWS =

  (SCREEN_H -
   (TEXT_MARGIN_Y * 2))

  / LINE_H;


constexpr int MAX_GLYPHS =
  MAX_COLS * MAX_ROWS;


// ============================================================
// TEXT DECODING SPEED
// ============================================================

// Lower = faster typing

constexpr uint32_t TYPE_INTERVAL_MS = 28;


// How quickly scrambled letters mutate

constexpr uint32_t SCRAMBLE_TICK_MS = 40;


// Number of unresolved characters trailing behind

constexpr int SCRAMBLE_TAIL = 6;


// ============================================================
// TEXT HOLD TIME
//
// Longer blocks now remain longer.
//
// HOLD = BASE + characters x PER_CHARACTER
// capped at MAX.
// ============================================================

constexpr uint32_t TEXT_HOLD_BASE_MS = 1100;

constexpr uint32_t TEXT_HOLD_PER_CHARACTER_MS = 20;

constexpr uint32_t TEXT_HOLD_MAX_MS = 4500;


// small black gap between pages

constexpr uint32_t PAGE_GAP_MS = 300;


// ============================================================
// GLITCH SETTINGS
// ============================================================

constexpr uint32_t GLITCH_MIN_INTERVAL_MS = 900;
constexpr uint32_t GLITCH_MAX_INTERVAL_MS = 2200;

constexpr uint32_t GLITCH_FLASH_MIN_MS = 45;
constexpr uint32_t GLITCH_FLASH_MAX_MS = 110;


// ============================================================
// GLITCH PALETTE
// ============================================================

const uint16_t GLITCH_RED     = 0xF800;
const uint16_t GLITCH_BLUE    = 0x001F;
const uint16_t GLITCH_GREEN   = 0x07E0;
const uint16_t GLITCH_MAGENTA = 0xF81F;
const uint16_t GLITCH_CYAN    = 0x07FF;
const uint16_t GLITCH_YELLOW  = 0xFFE0;
const uint16_t GLITCH_WHITE   = 0xFFFF;


const uint16_t GLITCH_PALETTE[] = {

  GLITCH_RED,
  GLITCH_BLUE,
  GLITCH_GREEN,
  GLITCH_MAGENTA,
  GLITCH_CYAN,
  GLITCH_YELLOW,
  GLITCH_WHITE
};


constexpr int GLITCH_COLOUR_COUNT =

  sizeof(GLITCH_PALETTE) /
  sizeof(GLITCH_PALETTE[0]);


// ============================================================
// SCRAMBLE CHARACTERS
// ============================================================

const char SCRAMBLE_CHARS[] =

  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "0123456789"
  "#/+*=<>[]{}:_-";


constexpr int SCRAMBLE_CHAR_COUNT =
  sizeof(SCRAMBLE_CHARS) - 1;


// ============================================================
// CURRENT TEXT COLOURS
// ============================================================

uint16_t currentBG = ST77XX_BLACK;
uint16_t currentFG = ST77XX_WHITE;


// ============================================================
// GLYPH
// ============================================================

struct GlyphCell {

  char finalChar;

  int16_t x;
  int16_t y;
};


GlyphCell glyphs[MAX_GLYPHS];


// ============================================================
// SCRIPT MARKER
// ============================================================

const char EYE_MARKER[] =
  "[[EYE]]";


// ============================================================
// SCRIPT
// ============================================================

const char* const SCRIPT[] = {

R"TXT(This 'device', connects via Wi-Fi to other devices in a network. This is one component (chapter) of what will become something like a 'distributed video essay', a new concept for me, similar to an 'installation essay' or 'spatial essay',)TXT",

EYE_MARKER,

R"TXT(chapters spread across multiple media, devices and space.)TXT",

EYE_MARKER,

R"TXT(It's been hard to get all the tech inside here. This work is so completely hidden that most people I have shown this to so far think I have acquired a small, retro-looking TV from somewhere.

This is, I suppose, testament to its design. As mentioned earlier, it is simultaneously retro and futuristic, which makes it look surprisingly modern, perhaps?

The word device comes from dividere, or divisus meaning 'divided' or 'separated'.)TXT",

EYE_MARKER,

R"TXT(in old French "devis" also meaning 'will' or 'order' and devise (meaning 'design' or 'plan'),

Over centuries, the meaning has shifted to something more like a 'contrived invention' or 'mechanical tool', eventually leading towards the modern associations with electronic hardware.)TXT",

EYE_MARKER
};


constexpr int SCRIPT_COUNT =
  sizeof(SCRIPT) /
  sizeof(SCRIPT[0]);


// ============================================================
// CONTRAST COLOUR
// ============================================================

uint16_t contrastColour(
  uint16_t colour
) {

  uint8_t r =
    ((colour >> 11) & 0x1F)
    * 255 / 31;

  uint8_t g =
    ((colour >> 5) & 0x3F)
    * 255 / 63;

  uint8_t b =
    (colour & 0x1F)
    * 255 / 31;


  uint16_t brightness =

    (
      (uint16_t)r * 299 +

      (uint16_t)g * 587 +

      (uint16_t)b * 114
    )

    / 1000;


  return

    brightness > 145

      ? ST77XX_BLACK
      : ST77XX_WHITE;
}


// ============================================================
// NORMAL HORIZONTAL GLITCH
// ============================================================

void drawHorizontalGlitch(
  uint16_t background,
  bool strong
) {

  tft.fillScreen(
    background
  );


  int bands =

    strong

      ? random(4, 10)
      : random(1, 4);


  for (
    int i = 0;
    i < bands;
    i++
  ) {

    int y =
      random(
        0,
        SCREEN_H
      );


    int h =

      strong

        ? random(1, 7)
        : random(1, 4);


    uint16_t colour =

      GLITCH_PALETTE[
        random(
          GLITCH_COLOUR_COUNT
        )
      ];


    // HORIZONTAL ONLY

    tft.fillRect(
      0,
      y,
      SCREEN_W,
      h,
      colour
    );
  }
}


// ============================================================
// EYE-SIGNAL HORIZONTAL INTERFERENCE
//
// Drawn OVER the moving eye.
// ============================================================

void drawEyeSignalInterference() {

  int bands =
    random(3, 9);


  for (
    int i = 0;
    i < bands;
    i++
  ) {

    int y =
      random(
        0,
        SCREEN_H
      );

    int h =
      random(
        1,
        8
      );


    // Mostly black cuts, occasionally colour.

    uint16_t colour;


    if (
      random(100) < 60
    ) {

      colour =
        ST77XX_BLACK;
    }

    else {

      colour =

        GLITCH_PALETTE[
          random(
            GLITCH_COLOUR_COUNT
          )
        ];
    }


    tft.fillRect(
      0,
      y,
      SCREEN_W,
      h,
      colour
    );
  }
}


// ============================================================
// DRAW CHARACTER
// ============================================================

void drawCell(
  const GlyphCell &cell,
  char character
) {

  tft.fillRect(
    cell.x,
    cell.y,
    CHAR_W,
    CHAR_H,
    currentBG
  );


  if (
    character == ' '
  ) {

    return;
  }


  tft.setCursor(
    cell.x,
    cell.y
  );


  tft.setTextColor(
    currentFG
  );


  tft.setTextSize(
    TEXT_SIZE
  );


  tft.write(
    character
  );
}


// ============================================================
// PAGE LINE COUNT
// ============================================================

int pageLineCount(
  const String &page
) {

  if (
    page.length() == 0
  ) {

    return 0;
  }


  int lines = 1;


  for (
    size_t i = 0;
    i < page.length();
    i++
  ) {

    if (
      page[i] == '\n'
    ) {

      lines++;
    }
  }


  return lines;
}


// ============================================================
// BUILD GLYPHS
// ============================================================

int buildGlyphs(
  const String &page
) {

  int count = 0;

  int row = 0;
  int col = 0;


  int lines =
    pageLineCount(
      page
    );


  int textHeight =
    lines *
    LINE_H;


  int usableHeight =

    SCREEN_H -

    (TEXT_MARGIN_Y * 2);


  int startY =

    TEXT_MARGIN_Y +

    (
      usableHeight -
      textHeight
    )

    / 2;


  if (
    startY <
    TEXT_MARGIN_Y
  ) {

    startY =
      TEXT_MARGIN_Y;
  }


  for (
    size_t i = 0;
    i < page.length();
    i++
  ) {

    char c =
      page[i];


    if (
      c == '\n'
    ) {

      row++;
      col = 0;


      if (
        row >=
        MAX_ROWS
      ) {

        break;
      }


      continue;
    }


    if (
      count >=
      MAX_GLYPHS
    ) {

      break;
    }


    glyphs[count].finalChar =
      c;


    glyphs[count].x =

      TEXT_MARGIN_X +

      (
        col *
        CHAR_W
      );


    glyphs[count].y =

      startY +

      (
        row *
        LINE_H
      );


    count++;
    col++;
  }


  return count;
}


// ============================================================
// REDRAW TEXT STATE
// ============================================================

void redrawTextState(
  int count,
  int stableCount,
  int progress
) {

  // ----------------------------------------------------------
  // STABLE CHARACTERS
  // ----------------------------------------------------------

  for (
    int i = 0;
    i < stableCount &&
    i < count;
    i++
  ) {

    drawCell(
      glyphs[i],
      glyphs[i].finalChar
    );
  }


  // ----------------------------------------------------------
  // SCRAMBLING CHARACTERS
  // ----------------------------------------------------------

  int activeEnd =

    progress < count

      ? progress
      : count;


  for (
    int i = stableCount;
    i < activeEnd;
    i++
  ) {

    if (
      glyphs[i].finalChar ==
      ' '
    ) {

      drawCell(
        glyphs[i],
        ' '
      );
    }

    else {

      char r =

        SCRAMBLE_CHARS[
          random(
            SCRAMBLE_CHAR_COUNT
          )
        ];


      drawCell(
        glyphs[i],
        r
      );
    }
  }
}


// ============================================================
// COLOUR GLITCH
// ============================================================

void glitchPulse(
  int count,
  int stableCount,
  int progress
) {

  uint16_t colour =

    GLITCH_PALETTE[
      random(
        GLITCH_COLOUR_COUNT
      )
    ];


  currentBG =
    colour;


  currentFG =
    contrastColour(
      currentBG
    );


  drawHorizontalGlitch(
    currentBG,
    true
  );


  redrawTextState(
    count,
    stableCount,
    progress
  );


  delay(
    random(
      GLITCH_FLASH_MIN_MS,
      GLITCH_FLASH_MAX_MS
    )
  );


  // ----------------------------------------------------------
  // RETURN TO BLACK
  // ----------------------------------------------------------

  currentBG =
    ST77XX_BLACK;


  currentFG =
    ST77XX_WHITE;


  drawHorizontalGlitch(
    ST77XX_BLACK,
    false
  );


  redrawTextState(
    count,
    stableCount,
    progress
  );
}


// ============================================================
// RAW EYE FRAME
//
// NO BYTE SWAP.
// ============================================================

bool loadRawFrame(
  int frameNumber
) {

  char filename[24];


  sprintf(
    filename,
    "/%04d.raw",
    frameNumber
  );


  File file =
    SD.open(
      filename,
      FILE_READ
    );


  if (
    !file
  ) {

    Serial.print(
      "Missing: "
    );

    Serial.println(
      filename
    );

    return false;
  }


  if (
    file.size() !=
    FRAME_BYTES
  ) {

    Serial.print(
      "Wrong size: "
    );

    Serial.println(
      filename
    );

    file.close();

    return false;
  }


  uint16_t *buffer =
    canvas->getBuffer();


  size_t bytesRead =

    file.read(
      (uint8_t *)buffer,
      FRAME_BYTES
    );


  file.close();


  if (
    bytesRead !=
    FRAME_BYTES
  ) {

    Serial.println(
      "Incomplete frame read"
    );

    return false;
  }


  // IMPORTANT:
  // NO BYTE SWAP

  return true;
}


// ============================================================
// PUSH EYE FRAME
// ============================================================

void pushEyeFrame() {

  tft.startWrite();


  tft.setAddrWindow(
    0,
    0,
    SCREEN_W,
    SCREEN_H
  );


  tft.writePixels(
    canvas->getBuffer(),
    SCREEN_W *
    SCREEN_H
  );


  tft.endWrite();
}


// ============================================================
// CIRCULAR FRAME DISTANCE
// ============================================================

int circularFrameDistance(
  int a,
  int b
) {

  int d =
    abs(
      a - b
    );


  if (
    d >
    FRAME_COUNT / 2
  ) {

    d =
      FRAME_COUNT - d;
  }


  return d;
}


// ============================================================
// RANDOM FILM START POINT
// ============================================================

int chooseEyeStart() {

  int candidate;


  do {

    candidate =

      FIRST_FRAME +

      (
        esp_random() %
        FRAME_COUNT
      );

  }

  while (

    lastEyeStart >= 0 &&

    circularFrameDistance(

      candidate -
      FIRST_FRAME,

      lastEyeStart -
      FIRST_FRAME

    )

    <

    MIN_EYE_START_SEPARATION
  );


  lastEyeStart =
    candidate;


  return candidate;
}


// ============================================================
// PLAY EYE FROM FRAME
// ============================================================

void playEyeFromFrame(
  int startFrame,
  uint32_t durationMs
) {

  int frame =
    startFrame;


  uint32_t started =
    millis();


  while (

    millis() -
    started <

    durationMs

  ) {

    uint32_t frameStarted =
      millis();


    if (
      loadRawFrame(
        frame
      )
    ) {

      pushEyeFrame();
    }


    frame++;


    if (
      frame >
      LAST_FRAME
    ) {

      frame =
        FIRST_FRAME;
    }


    uint32_t elapsed =

      millis() -
      frameStarted;


    if (
      elapsed <
      FRAME_TIME_MS
    ) {

      delay(
        FRAME_TIME_MS -
        elapsed
      );
    }
  }
}


// ============================================================
// NORMAL RANDOM EYE CLIP
//
// 3–10 seconds.
// ============================================================

void playRandomEyeClip() {

  int start =
    chooseEyeStart();


  uint32_t duration =

    random(
      EYE_MIN_SECONDS,
      EYE_MAX_SECONDS + 1
    )

    * 1000UL;


  Serial.print(
    "Eye start: "
  );

  Serial.print(
    start
  );

  Serial.print(
    " duration: "
  );

  Serial.print(
    duration
  );

  Serial.println(
    " ms"
  );


  playEyeFromFrame(
    start,
    duration
  );
}


// ============================================================
// ONE-SECOND EYE SIGNAL INTRUSION
//
// This is DIFFERENT from normal eye playback.
//
// Moving eye appears unexpectedly,
// horizontal interference cuts through it,
// then text continues where it left off.
// ============================================================

void playEyeSignalGlitch() {

  int frame =
    chooseEyeStart();


  uint32_t started =
    millis();


  Serial.println(
    "EYE SIGNAL INTRUSION"
  );


  while (

    millis() -
    started <

    EYE_INTRUSION_MS

  ) {

    uint32_t frameStarted =
      millis();


    if (
      loadRawFrame(
        frame
      )
    ) {

      pushEyeFrame();


      // eye image interrupted by
      // horizontal signal strips

      drawEyeSignalInterference();
    }


    frame++;


    if (
      frame >
      LAST_FRAME
    ) {

      frame =
        FIRST_FRAME;
    }


    uint32_t elapsed =

      millis() -
      frameStarted;


    if (
      elapsed <
      FRAME_TIME_MS
    ) {

      delay(
        FRAME_TIME_MS -
        elapsed
      );
    }
  }
}


// ============================================================
// CALCULATE TEXT HOLD
//
// Long text = longer pause.
// ============================================================

uint32_t calculateTextHold(
  int characterCount
) {

  uint32_t hold =

    TEXT_HOLD_BASE_MS +

    (
      characterCount *
      TEXT_HOLD_PER_CHARACTER_MS
    );


  if (
    hold >
    TEXT_HOLD_MAX_MS
  ) {

    hold =
      TEXT_HOLD_MAX_MS;
  }


  return hold;
}


// ============================================================
// ANIMATE ONE PAGE
// ============================================================

void animateTextPage(
  const String &page
) {

  currentBG =
    ST77XX_BLACK;

  currentFG =
    ST77XX_WHITE;


  tft.fillScreen(
    currentBG
  );


  int count =
    buildGlyphs(
      page
    );


  if (
    count <= 0
  ) {

    return;
  }


  int progress = 0;
  int lastStableCount = 0;


  uint32_t lastType =
    millis();


  uint32_t lastScramble =
    millis();


  uint32_t nextGlitch =

    millis() +

    random(
      GLITCH_MIN_INTERVAL_MS,
      GLITCH_MAX_INTERVAL_MS
    );


  int targetProgress =

    count +
    SCRAMBLE_TAIL;


  // ==========================================================
  // DECIDE WHETHER THIS PAGE GETS AN EYE INTERRUPTION
  // ==========================================================

  bool eyeIntrusionPending =

    random(100) <
    EYE_INTRUSION_PERCENT;


  uint32_t eyeIntrusionAt = 0;


  if (
    eyeIntrusionPending
  ) {

    uint32_t approximateTypingTime =

      count *
      TYPE_INTERVAL_MS;


    // Make sure intrusion occurs AFTER text has begun.

    uint32_t earliest =
      450;


    uint32_t latest =

      approximateTypingTime >
      900

        ? approximateTypingTime
        : 900;


    eyeIntrusionAt =

      millis() +

      random(
        earliest,
        latest
      );
  }


  // ==========================================================
  // DECODING
  // ==========================================================

  while (
    progress <=
    targetProgress
  ) {

    uint32_t now =
      millis();


    // --------------------------------------------------------
    // EYE SIGNAL INTERRUPTION
    // --------------------------------------------------------

    if (

      eyeIntrusionPending &&

      (int32_t)(
        now -
        eyeIntrusionAt
      ) >= 0

    ) {

      // Remember where decoding had reached.

      int stableCount =

        progress -
        SCRAMBLE_TAIL;


      if (
        stableCount < 0
      ) {

        stableCount = 0;
      }


      if (
        stableCount > count
      ) {

        stableCount =
          count;
      }


      // Eye breaks through.

      playEyeSignalGlitch();


      // ------------------------------------------------------
      // DROP STRAIGHT BACK TO TEXT
      // ------------------------------------------------------

      currentBG =
        ST77XX_BLACK;


      currentFG =
        ST77XX_WHITE;


      tft.fillScreen(
        ST77XX_BLACK
      );


      redrawTextState(
        count,
        stableCount,
        progress
      );


      eyeIntrusionPending =
        false;


      // Reset text/glitch clocks so decoding
      // continues naturally rather than jumping.

      lastType =
        millis();


      lastScramble =
        millis();


      nextGlitch =

        millis() +

        random(
          GLITCH_MIN_INTERVAL_MS,
          GLITCH_MAX_INTERVAL_MS
        );


      continue;
    }


    // --------------------------------------------------------
    // ADVANCE TEXT
    // --------------------------------------------------------

    if (
      now -
      lastType >=
      TYPE_INTERVAL_MS
    ) {

      lastType =
        now;


      progress++;


      int stableCount =

        progress -
        SCRAMBLE_TAIL;


      if (
        stableCount < 0
      ) {

        stableCount = 0;
      }


      if (
        stableCount > count
      ) {

        stableCount =
          count;
      }


      while (
        lastStableCount <
        stableCount
      ) {

        drawCell(

          glyphs[
            lastStableCount
          ],

          glyphs[
            lastStableCount
          ].finalChar

        );


        lastStableCount++;
      }
    }


    // --------------------------------------------------------
    // SCRAMBLE ACTIVE CHARACTERS
    // --------------------------------------------------------

    if (
      now -
      lastScramble >=
      SCRAMBLE_TICK_MS
    ) {

      lastScramble =
        now;


      int stableCount =

        progress -
        SCRAMBLE_TAIL;


      if (
        stableCount < 0
      ) {

        stableCount = 0;
      }


      if (
        stableCount > count
      ) {

        stableCount =
          count;
      }


      int activeEnd =

        progress < count

          ? progress
          : count;


      for (
        int i = stableCount;
        i < activeEnd;
        i++
      ) {

        if (
          glyphs[i].finalChar ==
          ' '
        ) {

          drawCell(
            glyphs[i],
            ' '
          );
        }

        else {

          char randomChar =

            SCRAMBLE_CHARS[
              random(
                SCRAMBLE_CHAR_COUNT
              )
            ];


          drawCell(
            glyphs[i],
            randomChar
          );
        }
      }
    }


    // --------------------------------------------------------
    // COLOUR GLITCH
    // --------------------------------------------------------

    if (
      (int32_t)(
        now -
        nextGlitch
      ) >= 0
    ) {

      int stableCount =

        progress -
        SCRAMBLE_TAIL;


      if (
        stableCount < 0
      ) {

        stableCount = 0;
      }


      if (
        stableCount > count
      ) {

        stableCount =
          count;
      }


      glitchPulse(
        count,
        stableCount,
        progress
      );


      nextGlitch =

        millis() +

        random(
          GLITCH_MIN_INTERVAL_MS,
          GLITCH_MAX_INTERVAL_MS
        );
    }


    delay(1);
  }


  // ==========================================================
  // COMPLETE TEXT
  // ==========================================================

  currentBG =
    ST77XX_BLACK;


  currentFG =
    ST77XX_WHITE;


  tft.fillScreen(
    ST77XX_BLACK
  );


  for (
    int i = 0;
    i < count;
    i++
  ) {

    drawCell(
      glyphs[i],
      glyphs[i].finalChar
    );
  }


  // ==========================================================
  // LENGTH-DEPENDENT HOLD
  // ==========================================================

  uint32_t holdTime =
    calculateTextHold(
      count
    );


  uint32_t holdStarted =
    millis();


  nextGlitch =

    millis() +

    random(
      GLITCH_MIN_INTERVAL_MS,
      GLITCH_MAX_INTERVAL_MS
    );


  while (

    millis() -
    holdStarted <

    holdTime

  ) {

    uint32_t now =
      millis();


    if (
      (int32_t)(
        now -
        nextGlitch
      ) >= 0
    ) {

      glitchPulse(
        count,
        count,
        count
      );


      nextGlitch =

        millis() +

        random(
          GLITCH_MIN_INTERVAL_MS,
          GLITCH_MAX_INTERVAL_MS
        );
    }


    delay(5);
  }


  // ==========================================================
  // GAP BETWEEN PAGES
  // ==========================================================

  tft.fillScreen(
    ST77XX_BLACK
  );


  delay(
    PAGE_GAP_MS
  );
}


// ============================================================
// WORD WRAPPING / PAGINATION
// ============================================================

bool nextWrappedPage(
  const char *source,
  size_t &position,
  String &page
) {

  page = "";


  page.reserve(
    MAX_GLYPHS +
    MAX_ROWS
  );


  int row = 0;
  int col = 0;


  while (
    source[position]
  ) {

    char c =
      source[position];


    // --------------------------------------------------------
    // CARRIAGE RETURN
    // --------------------------------------------------------

    if (
      c == '\r'
    ) {

      position++;

      continue;
    }


    // --------------------------------------------------------
    // SPACES
    // --------------------------------------------------------

    if (
      c == ' ' ||
      c == '\t'
    ) {

      position++;

      continue;
    }


    // --------------------------------------------------------
    // EXPLICIT LINE BREAK
    // --------------------------------------------------------

    if (
      c == '\n'
    ) {

      position++;


      if (
        row >=
        MAX_ROWS - 1
      ) {

        break;
      }


      page += '\n';

      row++;
      col = 0;

      continue;
    }


    // --------------------------------------------------------
    // FIND WORD
    // --------------------------------------------------------

    size_t wordStart =
      position;


    size_t wordEnd =
      position;


    while (

      source[wordEnd] &&

      source[wordEnd] != ' ' &&

      source[wordEnd] != '\t' &&

      source[wordEnd] != '\n' &&

      source[wordEnd] != '\r'

    ) {

      wordEnd++;
    }


    int wordLength =

      wordEnd -
      wordStart;


    // --------------------------------------------------------
    // START OF LINE
    // --------------------------------------------------------

    if (
      col == 0
    ) {

      int take =
        wordLength;


      if (
        take >
        MAX_COLS
      ) {

        take =
          MAX_COLS;
      }


      for (
        int i = 0;
        i < take;
        i++
      ) {

        page +=

          source[
            wordStart + i
          ];
      }


      col +=
        take;


      position +=
        take;


      if (
        take <
        wordLength
      ) {

        if (
          row >=
          MAX_ROWS - 1
        ) {

          break;
        }


        page += '\n';

        row++;
        col = 0;
      }


      continue;
    }


    // --------------------------------------------------------
    // WORD FITS CURRENT LINE
    // --------------------------------------------------------

    if (

      col +
      1 +
      wordLength <=
      MAX_COLS

    ) {

      page += ' ';

      col++;


      for (
        size_t i = wordStart;
        i < wordEnd;
        i++
      ) {

        page +=
          source[i];
      }


      col +=
        wordLength;


      position =
        wordEnd;


      continue;
    }


    // --------------------------------------------------------
    // WRAP TO NEXT LINE
    // --------------------------------------------------------

    if (
      row >=
      MAX_ROWS - 1
    ) {

      break;
    }


    page += '\n';

    row++;
    col = 0;
  }


  return
    page.length() > 0;
}


// ============================================================
// PLAY PASSAGE
// ============================================================

void playPassage(
  const char *text
) {

  size_t position = 0;


  while (
    text[position]
  ) {

    String page;


    size_t previousPosition =
      position;


    if (
      nextWrappedPage(
        text,
        position,
        page
      )
    ) {

      if (
        page.length() > 0
      ) {

        animateTextPage(
          page
        );
      }
    }


    // Safety

    if (
      position ==
      previousPosition
    ) {

      position++;
    }
  }
}


// ============================================================
// FINAL 30 SECOND EYE LOOP
// ============================================================

void playFinalEyeLoop() {

  Serial.println(
    "FINAL 30 SECOND EYE LOOP"
  );


  playEyeFromFrame(
    FIRST_FRAME,
    FINAL_EYE_LOOP_MS
  );


  tft.fillScreen(
    ST77XX_BLACK
  );


  delay(
    500
  );
}


// ============================================================
// ERROR
// ============================================================

void fatalError(
  const char *message
) {

  tft.fillScreen(
    ST77XX_BLACK
  );


  tft.setTextColor(
    ST77XX_WHITE
  );


  tft.setTextSize(2);


  tft.setCursor(
    18,
    25
  );


  tft.println(
    "ERROR"
  );


  tft.setTextSize(1);


  tft.setCursor(
    18,
    60
  );


  tft.println(
    message
  );


  while (
    true
  ) {

    delay(
      1000
    );
  }
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(
    115200
  );


  delay(
    1500
  );


  randomSeed(
    esp_random()
  );


  Serial.println();
  Serial.println(
    "=========================="
  );
  Serial.println(
    "VIEWER 2 / EYE / TEXT"
  );
  Serial.println(
    "=========================="
  );


  // ==========================================================
  // CHIP SELECT
  // ==========================================================

  pinMode(
    TFT_CS,
    OUTPUT
  );


  pinMode(
    SD_CS,
    OUTPUT
  );


  digitalWrite(
    TFT_CS,
    HIGH
  );


  digitalWrite(
    SD_CS,
    HIGH
  );


  // ==========================================================
  // SHARED SPI
  // ==========================================================

  SPI.begin(
    SPI_SCLK,
    SPI_MISO,
    SPI_MOSI,
    SD_CS
  );


  // ==========================================================
  // TFT
  // ==========================================================

  tft.init(
    240,
    320
  );


  tft.setRotation(
    3
  );


  tft.setSPISpeed(
    TFT_SPI_HZ
  );


  tft.setTextWrap(
    false
  );


  tft.fillScreen(
    ST77XX_BLACK
  );


  // IMPORTANT:
  // restore custom SPI bus after tft.init()

  SPI.begin(
    SPI_SCLK,
    SPI_MISO,
    SPI_MOSI,
    SD_CS
  );


  Serial.println(
    "LCD OK"
  );


  // ==========================================================
  // SD
  // ==========================================================

  if (
    !SD.begin(
      SD_CS,
      SPI,
      SD_SPI_HZ
    )
  ) {

    fatalError(
      "SD CARD FAILED"
    );
  }


  Serial.println(
    "SD OK"
  );


  // ==========================================================
  // CANVAS
  // ==========================================================

  canvas =
    new GFXcanvas16(
      SCREEN_W,
      SCREEN_H
    );


  if (

    !canvas ||

    !canvas->getBuffer()

  ) {

    fatalError(
      "CANVAS FAILED"
    );
  }


  canvas->fillScreen(
    ST77XX_BLACK
  );


  Serial.println(
    "CANVAS OK"
  );


  Serial.println(
    "READY"
  );


  delay(
    500
  );
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop() {


  // ==========================================================
  // SCRIPT
  // ==========================================================

  for (
    int i = 0;
    i < SCRIPT_COUNT;
    i++
  ) {


    // --------------------------------------------------------
    // FORMAL EYEBALL SECTION
    // --------------------------------------------------------

    if (

      strcmp(
        SCRIPT[i],
        EYE_MARKER
      ) == 0

    ) {

      playRandomEyeClip();

      continue;
    }


    // --------------------------------------------------------
    // TEXT
    //
    // Eye may now unexpectedly break into it.
    // --------------------------------------------------------

    playPassage(
      SCRIPT[i]
    );
  }


  // ==========================================================
  // FULL 30 SECOND FILM
  // ==========================================================

  playFinalEyeLoop();


  // Entire work begins again.
}