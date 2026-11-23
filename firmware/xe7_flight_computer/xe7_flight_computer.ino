#include <Servo.h>

#include <Wire.h>

#include <SPI.h>

#include <SD.h>

#include <Adafruit_GFX.h>

#include <Adafruit_SSD1306.h>

#include <Adafruit_MPU6050.h>

#include <Adafruit_Sensor.h>

// =====================================================

// DONANIM

// =====================================================

#define SCREEN_WIDTH 128

#define SCREEN_HEIGHT 64

#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_MPU6050 mpu;

Servo pitchServo;

Servo rollServo;

// Pinler - DEVRE DEĞİŞMİYOR

const int LED_MAVI = 5;

const int LED_YESIL = 6;

const int LED_KIRMIZI = 4;

const int BUZZER = 11;

const int ANA_PARASUT = 12;

const int DROGUE = 13;

const int ARM_SWITCH = 7;

const int SD_CS = 53;

// =====================================================

// UÇUŞ PARAMETRELERİ

// =====================================================

const float MAX_IRTIFA = 492.0;

const unsigned long APOGEE_ZAMANI = 7360;

const unsigned long INIS_ZAMANI = 47600;

const float KONTROLLU_INIS_IRTIFA = 150.0;

// Wokwi için basit ulaşılabilirlik katsayısı

const float ERISIM_KATSAYISI = 0.15;

// =====================================================

// GPS

// =====================================================

const float BASLANGIC_LAT = 40.4093;

const float BASLANGIC_LNG = 49.8671;

float gpsLat = BASLANGIC_LAT;

float gpsLng = BASLANGIC_LNG;

// =====================================================

// GÜVENLİ HEDEFLER

// =====================================================

// Hedef 0 = fırlatma alanı

// Hedef 1 = güvenli alternatif A

// Hedef 2 = güvenli alternatif B

//

// Bunlar sadece Wokwi simülasyon koordinatlarıdır.

const int HEDEF_SAYISI = 3;

float hedefLat[HEDEF_SAYISI] = {

  40.4093,

  40.4098,

  40.4088

};

float hedefLng[HEDEF_SAYISI] = {

  49.8671,

  49.8675,

  49.8667

};

bool hedefGuvenli[HEDEF_SAYISI] = {

  true,

  true,

  true

};

// =====================================================

// SİSTEM DEĞİŞKENLERİ

// =====================================================

int secilenHedef = 0;

float hedefMesafe = 0;

float ulasilabilirMesafe = 0;

String durum = "HAZIR";

String yon = "DUZ";

bool apogee = false;

bool inisTamamlandi = false;

bool geriDonusMumkun = false;

bool alternatifHedef = false;

bool sdHazir = false;

unsigned long sonLog = 0;
bool csvHeaderPrinted = false;

// =====================================================

// IMU

// =====================================================

float ivmeX = 0;

float ivmeY = 0;

float ivmeZ = 0;

float toplamIvme = 0;

// =====================================================

// MESAFE

// =====================================================

float mesafeHesapla(

  float lat1,

  float lng1,

  float lat2,

  float lng2

) {

  float dLat = (lat2 - lat1) * 111000.0;

  float dLng = (lng2 - lng1) * 111000.0;

  return sqrt(

    dLat * dLat +

    dLng * dLng

  );

}

// =====================================================

// GPS SİMÜLASYONU

// =====================================================

void gpsGuncelle(unsigned long zaman) {

  float p =

    zaman / (float)INIS_ZAMANI;

  if (p > 1.0)

    p = 1.0;

  // Kontrollü yatay hareket simülasyonu

  gpsLat =

    BASLANGIC_LAT + 0.0010 * p;

  gpsLng =

    BASLANGIC_LNG + 0.0007 * p;

}

// =====================================================

// IMU OKU

// =====================================================

void imuOku() {

  sensors_event_t a;

  sensors_event_t g;

  sensors_event_t temp;

  mpu.getEvent(

    &a,

    &g,

    &temp

  );

  ivmeX = a.acceleration.x;

  ivmeY = a.acceleration.y;

  ivmeZ = a.acceleration.z;

  toplamIvme =

    sqrt(

      ivmeX * ivmeX +

      ivmeY * ivmeY +

      ivmeZ * ivmeZ

    );

}

// =====================================================

// LED KONTROL

// =====================================================

void ledleriAyarla(

  bool mavi,

  bool yesil,

  bool kirmizi

) {

  digitalWrite(LED_MAVI, mavi);

  digitalWrite(LED_YESIL, yesil);

  digitalWrite(LED_KIRMIZI, kirmizi);

}

// =====================================================

// BUZZER

// =====================================================

void bip() {

  tone(BUZZER, 1000, 180);

}

// =====================================================

// PARAŞÜT ÇIKIŞLARINI KAPAT

// =====================================================

// Devrede pinler korunuyor.

// Yeni simülasyonda ana geri dönüş sistemi

// paraşüte dayanmıyor.

void kurtarmaCikislariniKapat() {

  digitalWrite(ANA_PARASUT, LOW);

  digitalWrite(DROGUE, LOW);

}

// =====================================================

// YÖN HESAPLA

// =====================================================

void yonHesapla() {

  float latFark =

    hedefLat[secilenHedef] - gpsLat;

  float lngFark =

    hedefLng[secilenHedef] - gpsLng;

  yon = "";

  if (lngFark > 0.00003)

    yon += "SAGA";

  else if (lngFark < -0.00003)

    yon += "SOLA";

  if (latFark > 0.00003) {

    if (yon.length() > 0)

      yon += "+";

    yon += "YUKARI";

  }

  else if (latFark < -0.00003) {

    if (yon.length() > 0)

      yon += "+";

    yon += "ASAGI";

  }

  if (yon.length() == 0)

    yon = "HEDEF UZERINDE";

}

// =====================================================

// EN UYGUN ALTERNATİF HEDEF

// =====================================================

void alternatifHedefSec(

  float irtifa

) {

  ulasilabilirMesafe =

    irtifa * ERISIM_KATSAYISI;

  float enKisa = 999999;

  int bulunanHedef = -1;

  for (

    int i = 1;

    i < HEDEF_SAYISI;

    i++

  ) {

    if (!hedefGuvenli[i])

      continue;

    float mesafe =

      mesafeHesapla(

        gpsLat,

        gpsLng,

        hedefLat[i],

        hedefLng[i]

      );

    if (

      mesafe <= ulasilabilirMesafe &&

      mesafe < enKisa

    ) {

      enKisa = mesafe;

      bulunanHedef = i;

    }

  }

  if (bulunanHedef >= 0) {

    secilenHedef =

      bulunanHedef;

    hedefMesafe =

      enKisa;

    alternatifHedef = true;

    durum =

      "ALTERNATIF HEDEF";

  }

  else {

    // Güvenli alternatif bulunamazsa

    // en yakın güvenli bölge seçilir.

    float enYakin = 999999;

    for (

      int i = 1;

      i < HEDEF_SAYISI;

      i++

    ) {

      if (!hedefGuvenli[i])

        continue;

      float d =

        mesafeHesapla(

          gpsLat,

          gpsLng,

          hedefLat[i],

          hedefLng[i]

        );

      if (d < enYakin) {

        enYakin = d;

        secilenHedef = i;

      }

    }

    hedefMesafe =

      enYakin;

    alternatifHedef = true;

    durum =

      "GUVENLI BOLGE";

  }

}

// =====================================================

// GERİ DÖNÜŞ KARARI

// =====================================================

void geriDonusKarari(

  float irtifa

) {

  float merkezMesafesi =

    mesafeHesapla(

      gpsLat,

      gpsLng,

      BASLANGIC_LAT,

      BASLANGIC_LNG

    );

  ulasilabilirMesafe =

    irtifa * ERISIM_KATSAYISI;

  // Fırlatma alanına dönülebiliyorsa

  if (

    merkezMesafesi <=

    ulasilabilirMesafe

  ) {

    geriDonusMumkun = true;

    alternatifHedef = false;

    secilenHedef = 0;

    hedefMesafe =

      merkezMesafesi;

    durum =

      "GERI DONUS";

  }

  // Dönüş mümkün değilse

  else {

    geriDonusMumkun = false;

    alternatifHedefSec(

      irtifa

    );

  }

  yonHesapla();

}

// =====================================================

// SERVO KANATÇIK KONTROLÜ

// =====================================================

void kanatlariYonet() {

  float latFark =

    hedefLat[secilenHedef] - gpsLat;

  float lngFark =

    hedefLng[secilenHedef] - gpsLng;

  int pitch = 90;

  int roll = 90;

  // Sola / sağa

  if (lngFark > 0.00003)

    roll = 110;

  else if (lngFark < -0.00003)

    roll = 70;

  // Yukarı / aşağı

  if (latFark > 0.00003)

    pitch = 103;

  else if (latFark < -0.00003)

    pitch = 77;

  // Hedefe çok yaklaştığında

  if (hedefMesafe < 20) {

    pitch = 90;

    roll = 90;

  }

  pitchServo.write(pitch);

  rollServo.write(roll);

  yonHesapla();

}

// =====================================================

// OLED

// =====================================================

void ekranGoster(

  float irtifa

) {

  display.clearDisplay();

  display.setTextSize(1);

  display.setTextColor(

    SSD1306_WHITE

  );

  display.setCursor(0, 0);

  display.print("IRT:");

  display.print(irtifa, 0);

  display.println("m");

  display.print("GPS:");

  display.println(

    gpsLat,

    4

  );

  display.print("   ");

  display.println(

    gpsLng,

    4

  );

  display.print("H:");

  display.print(

    secilenHedef

  );

  display.print(" D:");

  display.print(

    hedefMesafe,

    0

  );

  display.println("m");

  display.print("Y:");

  display.println(yon);

  display.print("S:");

  display.println(durum);

  display.display();

}

// =====================================================

// İNİŞ EKRANI

// =====================================================

void inisEkrani() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setTextColor(

    SSD1306_WHITE

  );

  display.setCursor(0, 5);

  display.println(

    "ROKET YERE INDI"

  );

  display.setCursor(0, 23);

  display.println(

    "HEDEF BOLGE"

  );

  display.setCursor(0, 41);

  display.println(

    "SISTEM DAYANDI"

  );

  display.display();

}

// =====================================================

// APOGEE

// =====================================================

void apogeeBaslat() {

  if (apogee)

    return;

  apogee = true;

  // Paraşüt çıkışları kapalı

  kurtarmaCikislariniKapat();

  ledleriAyarla(

    false,

    false,

    true

  );

  geriDonusKarari(

    MAX_IRTIFA

  );

  bip();

  Serial.println();

  Serial.println(

    "========== APOGEE =========="

  );

  Serial.print(

    "IRTIFA: "

  );

  Serial.println(

    MAX_IRTIFA

  );

  Serial.print(

    "HEDEF: "

  );

  Serial.println(

    secilenHedef

  );

  Serial.print(

    "MESAFE: "

  );

  Serial.print(

    hedefMesafe

  );

  Serial.println(

    " m"

  );

  if (geriDonusMumkun)

    Serial.println(

      "KARAR: GERI DONUS"

    );

  else

    Serial.println(

      "KARAR: ALTERNATIF HEDEF"

    );

}

// =====================================================

// KONTROLLÜ İNİŞ

// =====================================================

void kontrolluInis() {

  durum =

    "KONTROLLU INIS";

  ledleriAyarla(

    false,

    false,

    true

  );

  kanatlariYonet();

}

// =====================================================

// İNİŞ TAMAMLANDI

// =====================================================

void inisiBitir() {

  if (inisTamamlandi)

    return;

  inisTamamlandi = true;

  kurtarmaCikislariniKapat();

  pitchServo.write(90);

  rollServo.write(90);

  ledleriAyarla(

    false,

    true,

    false

  );

  durum =

    "INIS TAMAMLANDI";

  bip();

  Serial.println();

  Serial.println(

    "============================"

  );

  Serial.println(

    "     ROKET YERE INDI"

  );

  Serial.println(

    "     INIS TAMAMLANDI"

  );

  Serial.println(

    "     SISTEM DAYANDI"

  );

  Serial.println(

    "============================"

  );

  inisEkrani();

}

// =====================================================

// SD KART

// =====================================================

void sdLogla(

  unsigned long zaman,

  float irtifa

) {

  if (!sdHazir)

    return;

  File dosya =

    SD.open(

      "ucus.csv",

      FILE_WRITE

    );

  if (!dosya)

    return;

  dosya.print(zaman);

  dosya.print(",");

  dosya.print(irtifa, 1);

  dosya.print(",");

  dosya.print(gpsLat, 6);

  dosya.print(",");

  dosya.print(gpsLng, 6);

  dosya.print(",");

  dosya.print(secilenHedef);

  dosya.print(",");

  dosya.print(hedefMesafe, 1);

  dosya.print(",");

  dosya.print(yon);

  dosya.print(",");

  dosya.println(durum);

  dosya.close();

}

// =====================================================

// SETUP

// =====================================================

void setup() {

  Serial.begin(9600);

  // -------------------------

  // PINLER

  // -------------------------

  pinMode(

    LED_MAVI,

    OUTPUT

  );

  pinMode(

    LED_YESIL,

    OUTPUT

  );

  pinMode(

    LED_KIRMIZI,

    OUTPUT

  );

  pinMode(

    BUZZER,

    OUTPUT

  );

  pinMode(

    ANA_PARASUT,

    OUTPUT

  );

  pinMode(

    DROGUE,

    OUTPUT

  );

  pinMode(

    ARM_SWITCH,

    INPUT_PULLUP

  );

  // -------------------------

  // ÇIKIŞLAR

  // -------------------------

  kurtarmaCikislariniKapat();

  // -------------------------

  // SERVO

  // -------------------------

  pitchServo.attach(9);

  rollServo.attach(10);

  pitchServo.write(90);

  rollServo.write(90);

  // -------------------------

  // OLED

  // -------------------------

  if (

    !display.begin(

      SSD1306_SWITCHCAPVCC,

      0x3C

    )

  ) {

    Serial.println(

      "OLED HATA"

    );

  }

  // -------------------------

  // MPU6050

  // -------------------------

  if (!mpu.begin()) {

    Serial.println(

      "MPU6050 HATA"

    );

  } else {

    Serial.println(

      "MPU6050 HAZIR"

    );

  }

  // -------------------------

  // SD

  // -------------------------

  // SD KART
if (SD.begin(SD_CS)) {
  sdHazir = true;
  Serial.println("SD KART HAZIR");

  File dosya = SD.open("ucus.csv", FILE_WRITE);
  if (dosya) {
    if (dosya.size() == 0) {
      dosya.println("Zaman,Irtifa,Lat,Lng,Hedef,Mesafe,Yon,Durum");
    }
    dosya.close();
    Serial.println("ucus.csv HAZIR");
  } else {
    Serial.println("HATA: ucus.csv ACILMADI");
  }
} else {
  sdHazir = false;
  Serial.println("HATA: SD KART BASLATILAMADI");
}

// BAŞLANGIÇ LED

  // -------------------------

  ledleriAyarla(

    true,

    false,

    false

  );

  display.clearDisplay();

  display.setTextSize(1);

  display.setTextColor(

    SSD1306_WHITE

  );

  display.setCursor(0, 10);

  display.println(

    "AZROKSAN ROKET"

  );

  display.setCursor(0, 28);

  display.println(

    "OTONOM SISTEM"

  );

  display.setCursor(0, 46);

  display.println(

    "SISTEM HAZIR"

  );

  display.display();

  delay(1500);

}

// =====================================================

// LOOP

// =====================================================

void loop() {

  // ===================================================

  // ARM KAPALI

  // ===================================================

  if (

    digitalRead(ARM_SWITCH)

    == HIGH

  ) {

    ledleriAyarla(

      true,

      false,

      false

    );

    kurtarmaCikislariniKapat();

    pitchServo.write(90);

    rollServo.write(90);

    display.clearDisplay();

    display.setTextSize(1);

    display.setTextColor(

      SSD1306_WHITE

    );

    display.setCursor(0, 10);

    display.println(

      "SISTEM HAZIR"

    );

    display.setCursor(0, 30);

    display.println(

      "ARM ETMEK ICIN AC"

    );

    display.display();

    delay(250);

    return;

  }

  // ===================================================

  // İNİŞ BİTTİ

  // ===================================================

  if (inisTamamlandi) {

    pitchServo.write(90);

    rollServo.write(90);

    kurtarmaCikislariniKapat();

    ledleriAyarla(

      false,

      true,

      false

    );

    inisEkrani();

    delay(1000);

    return;

  }

  // ===================================================

  // ZAMAN

  // ===================================================

  unsigned long zaman =

    millis();

  unsigned long t =

    zaman;

  if (t >= INIS_ZAMANI)

    t = INIS_ZAMANI;

  // ===================================================

  // İRTİFA

  // ===================================================

  float irtifa;

  if (

    t <= APOGEE_ZAMANI

  ) {

    float p =

      t /

      (float)APOGEE_ZAMANI;

    irtifa =

      MAX_IRTIFA *

      sin(

        p * PI / 2.0

      );

  } else {

    float p =

      (

        t - APOGEE_ZAMANI

      )

      /

      (float)(

        INIS_ZAMANI -

        APOGEE_ZAMANI

      );

    if (p > 1.0)

      p = 1.0;

    irtifa =

      MAX_IRTIFA *

      (1.0 - p);

  }

  if (irtifa < 0)

    irtifa = 0;

  // ===================================================

  // SENSÖRLER

  // ===================================================

  gpsGuncelle(t);

  imuOku();

  // ===================================================

  // YÜKSELİŞ

  // ===================================================

  if (

    t < APOGEE_ZAMANI

  ) {

    durum =

      "YUKSELIS";

    ledleriAyarla(

      false,

      true,

      false

    );

    kurtarmaCikislariniKapat();

    pitchServo.write(90);

    rollServo.write(90);

    secilenHedef = 0;

    hedefMesafe =

      mesafeHesapla(

        gpsLat,

        gpsLng,

        BASLANGIC_LAT,

        BASLANGIC_LNG

      );

    yon =

      "YUKARI";

    ekranGoster(

      irtifa

    );

  }

  // ===================================================

  // APOGEE

  // ===================================================

  else if (

    !apogee

  ) {

    apogeeBaslat();

    ekranGoster(

      irtifa

    );

  }

  // ===================================================

  // İNİŞ

  // ===================================================

  else {

    // 150 m üstü:

    // otonom yönlendirme

    if (

      irtifa >

      KONTROLLU_INIS_IRTIFA

    ) {

      geriDonusKarari(

        irtifa

      );

      kontrolluInis();

      ekranGoster(

        irtifa

      );

    }

    // 150 m ve altı:

    // kontrollü son yaklaşma

    else if (

      irtifa > 0

    ) {

      kontrolluInis();

      ekranGoster(

        irtifa

      );

    }

    // 0 m:

    // iniş tamamlandı

    else {

      inisiBitir();

    }

  }

  // ===================================================

  // CSV TELEMETRIYA: SD karta ve Serial Monitor'a yaz
if (millis() - sonLog >= 500) {
  sonLog = millis();

  // SD karta kaydet
  sdLogla(t, irtifa);

  // CSV basligini Serial Monitor'da bir kez yaz
  if (!csvHeaderPrinted) {
    Serial.println("Zaman,Irtifa,Lat,Lng,Hedef,Mesafe,Yon,Durum");
    csvHeaderPrinted = true;
  }

  // CSV satiri
  Serial.print(t);
  Serial.print(",");
  Serial.print(irtifa, 1);
  Serial.print(",");
  Serial.print(gpsLat, 6);
  Serial.print(",");
  Serial.print(gpsLng, 6);
  Serial.print(",");
  Serial.print(secilenHedef);
  Serial.print(",");
  Serial.print(hedefMesafe, 1);
  Serial.print(",");
  Serial.print(yon);
  Serial.print(",");
  Serial.println(durum);
}

delay(100);

}
