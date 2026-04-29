#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSerif12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/Org_01.h>
#include <Fonts/Picopixel.h>
#include <Fonts/TomThumb.h>
#include <Preferences.h>
#include <SPI.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

// Pin map for Seeed Studio XIAO ESP32S3 exposed GPIOs.
// Change these if your board's exposed GPIO labels differ.
constexpr int EPD_SCK = 1;
constexpr int EPD_MOSI = 2;
constexpr int EPD_CS = 3;
constexpr int EPD_DC = 4;
constexpr int EPD_RST = 5;
constexpr int EPD_BUSY = 6;
constexpr int NEXT_BUTTON = 8;

constexpr uint32_t IDLE_SLEEP_MS = 90000;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 40;
constexpr uint32_t RESET_HOLD_MS = 5000;
constexpr int ROSARY_STEPS = 59;
constexpr int16_t FOOTER_RESERVE = 18;

// Waveshare 1.54" 200x200 black/white modules are commonly SSD1681-based.
// If your panel is an older revision, try GxEPD2_154 instead of GxEPD2_154_D67.
using Display154 = GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>;
Display154 display(GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

Preferences prefs;

struct RosaryState {
  uint16_t step;
  uint32_t completed;
};

enum class PrayerId : uint8_t {
  SignOfCross,
  ApostlesCreed,
  OurFather,
  HailMary,
  GloryBe,
};

const char PRAYER_SIGN_OF_CROSS[] =
    "In the name of the Father, and of the Son, and of the Holy Spirit. Amen.";

const char PRAYER_APOSTLES_CREED[] =
    "I believe in God, the Father almighty, Creator of Heaven and earth. And in Jesus Christ, His only Son, "
    "our Lord, Who was conceived by the Holy Spirit, born of the Virgin Mary, suffered under Pontius Pilate, "
    "was crucified, died, and was buried. He descended into Hell. The third day He rose again from the dead. "
    "He ascended into Heaven, and sits at the right hand of God, the Father almighty. He shall come again to "
    "judge the living and the dead. I believe in the Holy Spirit, the holy Catholic Church, the communion of "
    "saints, the forgiveness of sins, the resurrection of the body, and life everlasting. Amen.";

const char PRAYER_OUR_FATHER[] =
    "Our Father, who art in heaven, hallowed be thy name. Thy kingdom come, thy will be done, on earth as it "
    "is in heaven. Give us this day our daily bread, and forgive us our trespasses, as we forgive those who "
    "trespass against us. Lead us not into temptation, but deliver us from evil. Amen.";

const char PRAYER_HAIL_MARY[] =
    "Hail Mary, full of grace, the Lord is with thee. Blessed art thou among women, and blessed is the fruit "
    "of thy womb, Jesus. Holy Mary, Mother of God, pray for us sinners, now and at the hour of our death. Amen.";

const char PRAYER_GLORY_BE[] =
    "Glory be to the Father, and to the Son, and to the Holy Spirit. As it was in the beginning, is now, and "
    "ever shall be, world without end. Amen.";

struct PrayerConfig {
  PrayerId id;
  const char *name;
  const char *text;
  const GFXfont *font;
  uint8_t textSize;
  uint8_t lineHeight;
  int16_t paddingX;
  int16_t paddingTop;
  int16_t paddingBottom;
  uint16_t pagingWordCountCutoff;
};

const PrayerConfig PRAYER_CONFIGS[] = {
    {PrayerId::SignOfCross, "Sign of Cross", PRAYER_SIGN_OF_CROSS, &TomThumb, 2, 19, 10, 16, 10, 999},
    {PrayerId::ApostlesCreed, "Apostles Creed", PRAYER_APOSTLES_CREED, &TomThumb, 2, 19, 10, 16, 10, 40},
    {PrayerId::OurFather, "Our Father", PRAYER_OUR_FATHER, &TomThumb, 2, 19, 10, 16, 10, 40},
    {PrayerId::HailMary, "Hail Mary", PRAYER_HAIL_MARY, &TomThumb, 2, 19, 10, 16, 10, 999},
    {PrayerId::GloryBe, "Glory Be", PRAYER_GLORY_BE, &TomThumb, 2, 19, 10, 16, 10, 999},
};

RosaryState state{};
uint32_t lastButtonPressMs = 0;
uint32_t buttonPressStartMs = 0;
uint8_t currentPage = 0;
bool buttonWasPressed = false;
bool longPressHandled = false;
bool showingStartScreen = false;

PrayerId prayerIdForStep(uint16_t step) {
  if (step == 0) return PrayerId::SignOfCross;
  if (step == 1) return PrayerId::ApostlesCreed;
  if (step == 2) return PrayerId::OurFather;
  if (step >= 3 && step <= 5) return PrayerId::HailMary;
  if (step == 6) return PrayerId::GloryBe;

  const uint16_t decadeStep = (step - 7) % 10;
  if (decadeStep == 0) return PrayerId::OurFather;
  return PrayerId::HailMary;
}

const PrayerConfig &prayerConfigForId(PrayerId id) {
  for (const PrayerConfig &config : PRAYER_CONFIGS) {
    if (config.id == id) {
      return config;
    }
  }
  return PRAYER_CONFIGS[0];
}

const PrayerConfig &currentPrayerConfig() {
  return prayerConfigForId(prayerIdForStep(state.step));
}

uint8_t decadeForStep(uint16_t step) {
  if (step < 7) return 0;
  return ((step - 7) / 10) + 1;
}

uint8_t beadInDecade(uint16_t step) {
  if (step < 7) return step;
  return ((step - 7) % 10) + 1;
}

void loadState() {
  prefs.begin("rosary", false);
  state.step = prefs.getUShort("step", 0);
  state.completed = prefs.getUInt("done", 0);
  if (state.step >= ROSARY_STEPS) {
    state.step = 0;
  }
}

void saveState() {
  prefs.putUShort("step", state.step);
  prefs.putUInt("done", state.completed);
}

void advanceRosary() {
  currentPage = 0;
  state.step++;
  if (state.step >= ROSARY_STEPS) {
    state.step = 0;
    state.completed++;
  }
  saveState();
}

void restartRosary() {
  currentPage = 0;
  state.step = 0;
  saveState();
}

void drawCenteredText(const char *text, int16_t y, const GFXfont *font) {
  display.setFont(font);
  display.setTextSize(1);
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((display.width() - w) / 2, y);
  display.print(text);
}

uint16_t countWords(const char *text) {
  uint16_t count = 0;
  bool inWord = false;
  for (size_t i = 0; text[i] != '\0'; i++) {
    const bool isSpace = text[i] == ' ' || text[i] == '\n' || text[i] == '\t';
    if (!isSpace && !inWord) {
      count++;
      inWord = true;
    } else if (isSpace) {
      inWord = false;
    }
  }
  return count;
}

bool shouldPaginate(const PrayerConfig &config) {
  return countWords(config.text) >= config.pagingWordCountCutoff;
}

int16_t textX(const PrayerConfig &config) {
  return config.paddingX;
}

int16_t textY(const PrayerConfig &config) {
  return config.paddingTop;
}

int16_t textWidth(const PrayerConfig &config) {
  return display.width() - (config.paddingX * 2);
}

int16_t textMaxY(const PrayerConfig &config) {
  const int16_t arrowReserve = shouldPaginate(config) ? 22 : 0;
  return display.height() - config.paddingBottom - max<int16_t>(FOOTER_RESERVE, arrowReserve);
}

uint16_t countWrappedLines(const PrayerConfig &config) {
  display.setFont(config.font);
  display.setTextSize(config.textSize);

  char line[80] = "";
  char word[24] = "";
  size_t lineLen = 0;
  size_t wordLen = 0;
  uint16_t lineCount = 0;

  for (size_t i = 0;; i++) {
    const char c = config.text[i];
    const bool atBreak = c == ' ' || c == '\0';

    if (!atBreak && wordLen < sizeof(word) - 1) {
      word[wordLen++] = c;
      continue;
    }

    word[wordLen] = '\0';
    if (wordLen > 0) {
      char candidate[80];
      if (lineLen == 0) {
        snprintf(candidate, sizeof(candidate), "%s", word);
      } else {
        snprintf(candidate, sizeof(candidate), "%s %s", line, word);
      }

      int16_t x1;
      int16_t y1;
      uint16_t w;
      uint16_t h;
      display.getTextBounds(candidate, textX(config), textY(config), &x1, &y1, &w, &h);

      if (w > textWidth(config) && lineLen > 0) {
        lineCount++;
        snprintf(line, sizeof(line), "%s", word);
      } else {
        snprintf(line, sizeof(line), "%s", candidate);
      }
      lineLen = strlen(line);
      wordLen = 0;
    }

    if (c == '\0') {
      break;
    }
  }

  if (lineLen > 0) {
    lineCount++;
  }

  return lineCount;
}

uint8_t linesPerPage(const PrayerConfig &config) {
  return ((textMaxY(config) - textY(config)) / config.lineHeight) + 1;
}

bool prayerHasNextPage() {
  const PrayerConfig &config = currentPrayerConfig();
  if (!shouldPaginate(config)) {
    return false;
  }

  const uint16_t lineCount = countWrappedLines(config);
  return lineCount > static_cast<uint16_t>((currentPage + 1) * linesPerPage(config));
}

void drawWrappedTextPage(const PrayerConfig &config, uint8_t page) {
  display.setFont(config.font);
  display.setTextSize(config.textSize);
  display.setTextColor(GxEPD_BLACK);

  char line[80] = "";
  char word[24] = "";
  size_t lineLen = 0;
  size_t wordLen = 0;
  uint16_t lineIndex = 0;
  const uint16_t firstLine = shouldPaginate(config) ? page * linesPerPage(config) : 0;
  const uint16_t lineLimit = shouldPaginate(config) ? firstLine + linesPerPage(config) : UINT16_MAX;

  auto drawLineIfVisible = [&](const char *lineToDraw) {
    if (lineIndex >= firstLine && lineIndex < lineLimit) {
      display.setCursor(textX(config), textY(config) + ((lineIndex - firstLine) * config.lineHeight));
      display.print(lineToDraw);
    }
    lineIndex++;
  };

  for (size_t i = 0;; i++) {
    const char c = config.text[i];
    const bool atBreak = c == ' ' || c == '\0';

    if (!atBreak && wordLen < sizeof(word) - 1) {
      word[wordLen++] = c;
      continue;
    }

    word[wordLen] = '\0';
    if (wordLen > 0) {
      char candidate[80];
      if (lineLen == 0) {
        snprintf(candidate, sizeof(candidate), "%s", word);
      } else {
        snprintf(candidate, sizeof(candidate), "%s %s", line, word);
      }

      int16_t x1;
      int16_t y1;
      uint16_t w;
      uint16_t h;
      display.getTextBounds(candidate, textX(config), textY(config), &x1, &y1, &w, &h);

      if (w > textWidth(config) && lineLen > 0) {
        drawLineIfVisible(line);
        snprintf(line, sizeof(line), "%s", word);
      } else {
        snprintf(line, sizeof(line), "%s", candidate);
      }
      lineLen = strlen(line);
      wordLen = 0;
    }

    if (c == '\0') {
      break;
    }
  }

  if (lineLen > 0) {
    drawLineIfVisible(line);
  }
}

void drawMoreArrow() {
  display.fillTriangle(176, 184, 176, 196, 190, 190, GxEPD_BLACK);
}

bool progressLabel(char *label, size_t labelSize) {
  if (state.step >= 3 && state.step <= 5) {
    snprintf(label, labelSize, "%u/3", state.step - 2);
    return true;
  }

  if (state.step >= 7) {
    snprintf(label, labelSize, "%u/10", beadInDecade(state.step));
    return true;
  }

  return false;
}

void drawProgressLabel() {
  char label[8];
  if (!progressLabel(label, sizeof(label))) {
    return;
  }

  display.setFont(&FreeMono9pt7b);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK);

  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  display.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(display.width() - w - 10, display.height() - 8);
  display.print(label);
}

void drawStartImage() {
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);
    display.drawRect(0, 0, display.width(), display.height(), GxEPD_BLACK);

    display.fillRect(93, 34, 14, 82, GxEPD_BLACK);
    display.fillRect(64, 58, 72, 14, GxEPD_BLACK);

    display.drawCircle(100, 138, 5, GxEPD_BLACK);
    display.drawCircle(82, 134, 5, GxEPD_BLACK);
    display.drawCircle(67, 123, 5, GxEPD_BLACK);
    display.drawCircle(58, 106, 5, GxEPD_BLACK);
    display.drawCircle(57, 88, 5, GxEPD_BLACK);
    display.drawCircle(143, 88, 5, GxEPD_BLACK);
    display.drawCircle(142, 106, 5, GxEPD_BLACK);
    display.drawCircle(133, 123, 5, GxEPD_BLACK);
    display.drawCircle(118, 134, 5, GxEPD_BLACK);
  } while (display.nextPage());
}

void drawRosary() {
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);
    display.drawRect(0, 0, display.width(), display.height(), GxEPD_BLACK);

    drawWrappedTextPage(currentPrayerConfig(), currentPage);
    if (prayerHasNextPage()) {
      drawMoreArrow();
    }
    drawProgressLabel();
  } while (display.nextPage());

}

bool buttonPressed() {
  if (digitalRead(NEXT_BUTTON) == HIGH) {
    return false;
  }
  delay(BUTTON_DEBOUNCE_MS);
  return digitalRead(NEXT_BUTTON) == LOW;
}

void handleButtonPresses() {
  const bool pressed = digitalRead(NEXT_BUTTON) == LOW;
  if (pressed && !buttonWasPressed) {
    delay(BUTTON_DEBOUNCE_MS);
    if (digitalRead(NEXT_BUTTON) == LOW) {
      buttonPressStartMs = millis();
      lastButtonPressMs = millis();
      buttonWasPressed = true;
      longPressHandled = false;
    }
  } else if (pressed && buttonWasPressed) {
    lastButtonPressMs = millis();
    if (!longPressHandled && millis() - buttonPressStartMs >= RESET_HOLD_MS) {
      restartRosary();
      showingStartScreen = true;
      drawStartImage();
      longPressHandled = true;
    }
  } else if (!pressed && buttonWasPressed) {
    delay(BUTTON_DEBOUNCE_MS);
    if (digitalRead(NEXT_BUTTON) == HIGH) {
      if (!longPressHandled) {
        if (showingStartScreen) {
          showingStartScreen = false;
        } else if (prayerHasNextPage()) {
          currentPage++;
        } else {
          advanceRosary();
          showingStartScreen = state.step == 0;
        }
        if (showingStartScreen) {
          drawStartImage();
        } else {
          drawRosary();
        }
        lastButtonPressMs = millis();
      }
      buttonWasPressed = false;
    }
  }
}

void waitForButtonRelease() {
  while (digitalRead(NEXT_BUTTON) == LOW) {
    delay(10);
  }
  delay(BUTTON_DEBOUNCE_MS);
}

void sleepUntilButton() {
  pinMode(NEXT_BUTTON, INPUT_PULLUP);
  waitForButtonRelease();
  display.hibernate();
  rtc_gpio_pullup_en(static_cast<gpio_num_t>(NEXT_BUTTON));
  rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(NEXT_BUTTON));
  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(NEXT_BUTTON), 0);
  Serial.flush();
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(NEXT_BUTTON, INPUT_PULLUP);
  loadState();

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.init(115200, true, 2, false);

  const esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
  Serial.printf("Wakeup cause: %d, button: %s\n",
                wakeupCause,
                buttonPressed() ? "pressed" : "released");

  if (wakeupCause == ESP_SLEEP_WAKEUP_EXT0 && buttonPressed()) {
    buttonWasPressed = true;
    buttonPressStartMs = millis();
    longPressHandled = false;
  }

  showingStartScreen = state.step == 0;
  if (showingStartScreen) {
    drawStartImage();
  } else {
    drawRosary();
  }
  lastButtonPressMs = millis();
}

void loop() {
  handleButtonPresses();

  if (millis() - lastButtonPressMs >= IDLE_SLEEP_MS) {
    sleepUntilButton();
  }

  delay(10);
}
