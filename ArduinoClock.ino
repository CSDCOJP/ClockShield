#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <FastLED.h>

const int RCE = 2;      // RST
const int RDAT = 3;     // DAT
const int RCLK = 4;     // CLK
const int LED_DATA  = 8; // LED SINGAL
const int KEYADJ    = 9;  // ADJ KEY
const int KEYMINUS  = 10; // - KEY
const int KEYPLUS   = 11; // + KEY
const int KEYENTER  = 12; // ENTER KEY
const int LEDOUT    = 13; // LED

ThreeWire RtcWire(RDAT, RCLK, RCE); 
RtcDS1302<ThreeWire> Rtc(RtcWire);
const uint32_t RTCPOLLTM  = 100;       //100msに一度RTCから読み出し

RtcDateTime NTime, LTime;      // 調整用、現時刻、表示時間
uint8_t AdjHour;
uint8_t AdjMin;
uint8_t AdjSec;
uint8_t CompileYear;

uint32_t LMillis;

#define LED_WIDTH   32
#define LED_LINEWIDTH  5       
#define LED_HEIGHT  8       
#define NUM_LEDS    (LED_WIDTH * LED_HEIGHT)    // 横32 * 8縦

typedef struct {
  uint8_t data[LED_HEIGHT];
} TSeg;

#define  SEG_MAX    6
#ifdef ARDUINO
TSeg    TImage[SEG_MAX];
TSeg    NImage[SEG_MAX];
uint8_t ImageShiftCount[SEG_MAX];
#else
TSeg    TImage[65535];
TSeg    NImage[65535];
#endif

CRGB    NLeds[NUM_LEDS];	     // Led Color 情報

CRGB 	SetColor;                 // 時分秒色
CRGB 	SetColColor;              // コロン色
CRGB  CurrentColor;

bool    BlinkFlag;
uint8_t Brightness = 30;      //初期値は30で使用

uint8_t ColorMode   = 0;

uint8_t LKeyBits;
#define SEG_MAX 6
#define PT_SECL 5
#define PT_SECH 4
#define PT_MINL 3
#define PT_MINH 2
#define PT_HOURL 1
#define PT_HOURH 0


#define ADJ_OFF     0
#define ADJ_HOUR    1
#define ADJ_MIN     2
#define ADJ_SEC     3
uint8_t AdjustMode  = ADJ_OFF;


CRGB colorList[] = {
  CRGB::White,    //0
  CRGB::Red,      //1
  CRGB::Green,    //2
  CRGB::Blue,     //3
  CRGB::Yellow,   //4
  CRGB::Cyan,     //5
  CRGB::Magenta,    //6
  CRGB::Orange,   //7
  CRGB::Purple,   //8
  CRGB::Pink,   //9
  CRGB::Lime,   //10
  CRGB::Teal,   //11
  CRGB::Violet,   //12
  CRGB::Brown,    //13
  CRGB::Gray,      //14
  CRGB::DarkOrange  //15
};

#define FontArray(a,b,c,d,e,f,g,h) { a, b, c, d, e, f, g, h }
/* *********************** */
/* フォントデータ　         */
/* *********************** */
const TSeg MonoFontTable[] = {
    //       0     1    2    3    4    5    6   7
  FontArray(0x0E,0x11,0x11,0x11,0x11,0x11,0x11,0x0E), // 0
  FontArray(0x04,0x0C,0x04,0x04,0x04,0x04,0x04,0x0E), // 1
  FontArray(0x0E,0x11,0x01,0x01,0x02,0x04,0x08,0x1F), // 2
  FontArray(0x0E,0x11,0x01,0x0E,0x01,0x01,0x11,0x0E), // 3
  FontArray(0x02,0x06,0x0A,0x12,0x12,0x1F,0x02,0x02), // 4
  FontArray(0x1F,0x10,0x10,0x1E,0x01,0x01,0x11,0x0E), // 5
  FontArray(0x0E,0x10,0x10,0x1E,0x11,0x11,0x11,0x0E), // 6
  FontArray(0x1F,0x01,0x01,0x02,0x02,0x04,0x04,0x04), // 7
  FontArray(0x0E,0x11,0x11,0x0E,0x11,0x11,0x11,0x0E), // 8
  FontArray(0x0E,0x11,0x11,0x0F,0x01,0x01,0x11,0x0E), // 9
  FontArray(0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), // A 消灯
};

/* ****************************************************** */
/* * 現時刻 Printf確認                                    */
/* ****************************************************** */
void PrintDateTime(const RtcDateTime& data) 
{
  char datestring[3+3+3+1];

  snprintf_P(datestring,
             sizeof(datestring),
             PSTR("%02u:%02u:%02u"), data.Hour(), data.Minute(), data.Second());
  Serial.println(datestring);
}
/* ****************************************************** */
/* * 列ジグザグ（偶数列は下→上、奇数列は上→下）            */
/* ****************************************************** */
int xyToIndex(int col, int y) {
  // 列ジグザグ（偶数列は下→上、奇数列は上→下）
  return (col % 2 == 0)
    ? col * LED_HEIGHT + (LED_HEIGHT - 1 - y)
    : col * LED_HEIGHT + y;
}

/* ****************************************************** */
/* * 5x8 グリフ描画                                       */
/* ****************************************************** */
void draw5x8Glyph(const uint8_t glyph[8], int base_col, const CRGB &on)
{
  for (int y = 0; y < LED_HEIGHT; y++) 
  {
    uint8_t row = glyph[y];
    for (int x = 0; x < LED_LINEWIDTH; x++)
    {
      bool isOn = (row >> (4 - x)) & 0x01;
      int real_col = base_col + (4 - x);
      NLeds[xyToIndex(real_col, y)] = isOn ? on : CRGB::Black;
    }
  }
}
/* ****************************************************** */
/* * コロン（縦1列）描画：y==2,5 を点灯                    */
/* ****************************************************** */
void drawColonColumn(int col, const CRGB& dotColor) 
{
  for (int y = 0; y < LED_HEIGHT; y++) 
  {
    NLeds[xyToIndex(col, y)] = ((y == 2 || y == 5)) ? dotColor : CRGB::Black;
  }
}
/* ****************************************************** */
/* * Led Image BitSet                                     */
/* ****************************************************** */
void showTimeFromNImage()
{
  FastLED.clear(); // 全クリア

  int col = 0; // 列0からスタート (左端)

  CurrentColor = colorList[ColorMode];
 
  if(CurrentColor.g >= 128)
  {
    SetColColor = CRGB(
      CurrentColor.r,                            // Rそのまま
      constrain(CurrentColor.g - 100, 0, 255),   // Gに-100（255でクリップ）
      CurrentColor.b                             // Bそのまま
    );
  }else{
    SetColColor = CRGB(
      CurrentColor.r,                            // Rそのまま
      constrain(CurrentColor.g + 100, 0, 255),   // Gに+100（255でクリップ）
      CurrentColor.b                             // Bそのまま
    );
  }

  SetColor = CRGB(
  constrain(CurrentColor.r , 0, 255),
  constrain(CurrentColor.g , 0, 255),
  constrain(CurrentColor.b , 0, 255)
  );

    // ==== 秒下位 NImage[5] ====
    draw5x8Glyph(NImage[PT_SECL].data, col, SetColor);
    col += 5;

    // ==== 秒上位 NImage[4] ====
    draw5x8Glyph(NImage[PT_SECH].data, col, SetColor);
    col += 5;

    // ==== コロン（列10） ====
    drawColonColumn(col, SetColColor );
    col += 1;

    // ==== 分下位 NImage[3] ====
    draw5x8Glyph(NImage[PT_MINL].data, col, SetColor);
    col += 5;

    // ==== 分上位 NImage[2] ====
    draw5x8Glyph(NImage[PT_MINH].data, col, SetColor);
    col += 5;

    // ==== コロン（列21） ====
    drawColonColumn(col, SetColColor );
    col += 1;

    // ==== 時下位 NImage[1] ====
    draw5x8Glyph(NImage[PT_HOURL].data, col, SetColor );
    col += 5;

    // ==== 時上位 NImage[0] ====
    draw5x8Glyph(NImage[PT_HOURH].data, col, SetColor );

    FastLED.setBrightness(Brightness);
    FastLED.show(); // 最後に一括出力
}
/*******************************************/
/*   SSSS                                  */
/*  SS  SS            tt                   */
/*  SS       eeee   tttttt  uu  uu  ppppp  */
/*   SSSS   ee  ee    tt    uu  uu  pp  pp */
/*      SS  eeeeee    tt    uu  uu  pp  pp */
/*  SS  SS  ee        tt    uu  uu  ppppp  */
/*   SSSS    eeee      ttt   uuuuu  pp     */
/*                                  pp     */
/*******************************************/
void setup()
{
  Serial.begin(9600);
  pinMode(KEYADJ   ,  INPUT_PULLUP);
  pinMode(KEYMINUS , INPUT_PULLUP);
  pinMode(KEYPLUS  , INPUT_PULLUP);
  pinMode(KEYENTER , INPUT_PULLUP);
  pinMode(LEDOUT   , OUTPUT);

  int i = 0;
  while (!Serial) {
    i++;
    if(i >= 10000) break;
    ; // シリアルポートが開くのを待つ
  }
  delay(100);
  Serial.println("For Clock Shiled Ver 1.22");


  Serial.print("compiled: ");
  Serial.println(__TIME__);
  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime("Jan 1 2025", __TIME__);     // コンパイルした日として2025/1/1とする。

  Rtc.SetIsWriteProtected(false);
  Rtc.SetIsRunning(true);
  LMillis = millis();
  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) {
    Rtc.SetDateTime(compiled);
  } 
  Serial.print(now.Year());
  Serial.print(" ");
  Serial.print(now.Month());
  Serial.print(" ");
  Serial.println(now.Day());

  memset(NLeds,0,sizeof(NLeds));

  FastLED.addLeds<WS2812, LED_DATA, GRB>(NLeds, NUM_LEDS);
  FastLED.setBrightness(Brightness);
  FastLED.clear();
  FastLED.show();
}


void NImageCopy(void)
{
  for(int i= 0 ; i < SEG_MAX ; i ++)
  {
    if( memcmp (&TImage[i],&NImage[i],sizeof(NImage[0])) != 0)
    {
        if(ImageShiftCount[i] == 0) continue;
        for (int j = 7; j > 0; j--)
        {
            NImage[i].data[j] = NImage[i].data[j-1];
        }
        NImage[i].data[0] = TImage[i].data[ImageShiftCount[i]-1];
       if(ImageShiftCount[i])ImageShiftCount[i]--;
    }
  }
}

void NImageAllCopy(void)
{
  for(int i= 0 ; i < SEG_MAX ; i ++)
  {
    memcpy(&NImage[i], &TImage[i], sizeof(NImage[0]));
    ImageShiftCount[i] = 0; 
  }
}
/***************************/
/* KEY UP    ***************/
/***************************/
void KeyUp(void)
{
      switch(AdjustMode)
      {
          case ADJ_HOUR:
                  BlinkFlag   = true;
                  AdjHour ++;
                  if(AdjHour >= 24) AdjHour = 0;
                  break;
          case ADJ_MIN:
                  BlinkFlag   = true;
                  AdjMin ++;
                  if(AdjMin >= 60) AdjMin = 0;
                  break;
          case ADJ_SEC:
                  BlinkFlag   = true;
                  AdjSec ++;
                  if(AdjSec >= 60) AdjSec = 0;
                  break;
          default:
                  if( Brightness <  10 ) Brightness = 10;
                  else                   Brightness +=  10;
                  if( Brightness >= 50 ) Brightness = 50;     
       }
   Serial.print("Brightness");
   Serial.println(Brightness);
}
/***************************/
/* KEY DOWN  ***************/
/***************************/
void KeyDown(void)
{
    switch(AdjustMode)
    {
          case ADJ_HOUR:
                  BlinkFlag   = true;
                  AdjHour --;
                  if(AdjHour >= 24) AdjHour = 23;
                  break;
          case ADJ_MIN:
                  BlinkFlag   = true;
                  AdjMin --;
                  if(AdjMin >= 60) AdjMin = 59;
                  break;
          case ADJ_SEC:
                  BlinkFlag   = true;
                  AdjSec --;
                  if(AdjSec >= 60) AdjSec = 59;
                  break;
          default:
                  if( Brightness <=  10 ) Brightness = 1;
                  else
                  if( Brightness >   50) Brightness = 50; 
                  else                   Brightness -= 10; 
     } 
   Serial.print("Brightness");
   Serial.println(Brightness);
}
/***************************/
/*  KK  KK                 */
/*  KK KK                  */
/*  KKKK     eeee   yy  yy */
/*  KKK     ee  ee  yy  yy */
/*  KKKK    eeeeee  yy  yy */
/*  KK KK   ee       yyyyy */
/*  KK  KK   eeee      yy  */
/*                  yyyy   */
/***************************/
void Key(void)
{
  uint8_t KeyBits1 = 0;

  KeyBits1 |= (digitalRead(KEYADJ)    == LOW ? 1 : 0) << 3;
  KeyBits1 |= (digitalRead(KEYMINUS)  == LOW ? 1 : 0) << 2;
  KeyBits1 |= (digitalRead(KEYPLUS)   == LOW ? 1 : 0) << 1;
  KeyBits1 |= (digitalRead(KEYENTER)  == LOW ? 1 : 0) << 0;
  
  delay(10);  

  uint8_t KeyBits2 = 0;

  KeyBits2 |= (digitalRead(KEYADJ)    == LOW ? 1 : 0) << 3;
  KeyBits2 |= (digitalRead(KEYMINUS)  == LOW ? 1 : 0) << 2;
  KeyBits2 |= (digitalRead(KEYPLUS)   == LOW ? 1 : 0) << 1;
  KeyBits2 |= (digitalRead(KEYENTER)  == LOW ? 1 : 0) << 0;
  
  if(KeyBits1 != KeyBits2) return;
  
  if(LKeyBits != 0)
  {
      LKeyBits = KeyBits1;
      return;
  }
  
  LKeyBits = KeyBits1;
  if( KeyBits1 == 0x01 )
  {
      if( AdjustMode == ADJ_HOUR )
      {
        AdjustMode++;
      }else
      if( AdjustMode == ADJ_MIN )
      {
        AdjustMode++;
      }else
      if( AdjustMode == ADJ_SEC )
      {
        NTime = RtcDateTime(2025, 01, 02,AdjHour, AdjMin, AdjSec);      //日付を1/2にして、コンパイルした時間に戻るを防ぐ。
        Rtc.SetDateTime(NTime);
        AdjustMode = 0;
      }else{
          AdjustMode = 0;  
      }
  }else
  if( KeyBits1 == 0x02 )
  {   /* + KEYの処理　*/
      KeyUp();
  }else
  if( KeyBits1 == 0x04 )
  {
      KeyDown(); 
  }else
  if( KeyBits1 == 0x08 )
  {
      AdjustMode++;
      if( AdjustMode == ADJ_HOUR )
      {
        AdjHour = NTime.Hour();
        AdjMin  = NTime.Minute();
        AdjSec  = NTime.Second();
      }else
      if( AdjustMode  )
      {
          AdjustMode = 0;
      }
  }
}
/*******************************************************************/
/*  MM   MM                         LL                             */
/*  MMM MMM           ii            LL                             */
/*  MMMMMMM  aaaa           nnnnn   LL       oooo    oooo   ppppp  */
/*  MM M MM     aa   iii    nn  nn  LL      oo  oo  oo  oo  pp  pp */
/*  MM   MM  aaaaa    ii    nn  nn  LL      oo  oo  oo  oo  pp  pp */
/*  MM   MM aa  aa    ii    nn  nn  LL      oo  oo  oo  oo  ppppp  */
/*  MM   MM  aaaaa   iiii   nn  nn  LLLLLL   oooo    oooo   pp     */
/*******************************************************************/
void loop() 
{
  delay(40);  // five Second
  
  if( (uint32_t)millis() >= uint32_t(LMillis+RTCPOLLTM))
  {
     BlinkFlag = !BlinkFlag;
     LMillis   =  millis();
     NTime = Rtc.GetDateTime();
//   PrintDateTime(NTime); 
  }
  digitalWrite(LEDOUT,BlinkFlag);
  Key();
  if(AdjustMode == ADJ_HOUR)
  {
      if(BlinkFlag == false)
      {
        TImage[PT_HOURH] = MonoFontTable[0x0a]; 
        TImage[PT_HOURL] = MonoFontTable[0x0a]; 
      }
     else
      {
          if(AdjHour / 10) 
          {
            TImage[PT_HOURH] = MonoFontTable[AdjHour / 10]; 
          }else{
            TImage[PT_HOURH] = MonoFontTable[0x0a];
          }
          TImage[PT_HOURL] =   MonoFontTable[AdjHour  % 10]; 
      }
      TImage[PT_MINH] =   MonoFontTable[AdjMin / 10]; 
      TImage[PT_MINL] =   MonoFontTable[AdjMin % 10]; 

      TImage[PT_SECH] =   MonoFontTable[AdjSec / 10]; 
      TImage[PT_SECL] =   MonoFontTable[AdjSec % 10]; 

      memset(ImageShiftCount,0x08,sizeof(ImageShiftCount));

      NImageAllCopy();
      showTimeFromNImage();
      return;
  }else
  if(AdjustMode == ADJ_MIN)
  {
      if(AdjHour / 10) 
      {
        TImage[PT_HOURH] = MonoFontTable[AdjHour / 10]; 
      }else{
        TImage[PT_HOURH] = MonoFontTable[0x0a];
      }
      TImage[PT_HOURL] =   MonoFontTable[AdjHour  % 10]; 

      if(BlinkFlag == false)
      {
        TImage[PT_MINH] = MonoFontTable[0x0a]; 
        TImage[PT_MINL] = MonoFontTable[0x0a]; 
      }else{
        TImage[PT_MINH] =   MonoFontTable[AdjMin / 10]; 
        TImage[PT_MINL] =   MonoFontTable[AdjMin % 10]; 
      }
      TImage[PT_SECH] =   MonoFontTable[AdjSec / 10]; 
      TImage[PT_SECL] =   MonoFontTable[AdjSec % 10]; 

      memset(ImageShiftCount,0x08,sizeof(ImageShiftCount));

      NImageAllCopy();
      showTimeFromNImage();
      return;
  }else
  if(AdjustMode == ADJ_SEC)
  {
      if(AdjHour / 10) 
      {
        TImage[PT_HOURH] = MonoFontTable[AdjHour / 10]; 
      }else{
        TImage[PT_HOURH] = MonoFontTable[0x0a];
      }
      TImage[PT_HOURL] =   MonoFontTable[AdjHour  % 10]; 

      TImage[PT_MINH] =   MonoFontTable[AdjMin / 10]; 
      TImage[PT_MINL] =   MonoFontTable[AdjMin % 10]; 

      if(BlinkFlag == false)
      {
        TImage[PT_SECH] = MonoFontTable[0x0a]; 
        TImage[PT_SECL] = MonoFontTable[0x0a]; 
      }else{
        TImage[PT_SECH] =   MonoFontTable[AdjSec / 10]; 
        TImage[PT_SECL] =   MonoFontTable[AdjSec % 10]; 
      }
      memset(ImageShiftCount,0x08,sizeof(ImageShiftCount));

      NImageAllCopy();
      showTimeFromNImage();
      return;
  }else
  if (memcmp(&NTime,&LTime,sizeof(NTime)) != 0)
  {
        if(LTime.Minute() != NTime.Minute())
        {
          ColorMode++;
          ColorMode &= 0x0f;
      }
      memcpy(&LTime,&NTime,sizeof(LTime));
   
      if(LTime.Hour() / 10) 
      {
        TImage[PT_HOURH] = MonoFontTable[LTime.Hour() / 10]; 
      }else{
        TImage[PT_HOURH] = MonoFontTable[0x0a];
      }
      TImage[PT_HOURL] =   MonoFontTable[LTime.Hour()  % 10]; 

      TImage[PT_MINH] =   MonoFontTable[LTime.Minute() / 10]; 
      TImage[PT_MINL] =   MonoFontTable[LTime.Minute() % 10]; 

      TImage[PT_SECH] =   MonoFontTable[LTime.Second() / 10]; 
      TImage[PT_SECL] =   MonoFontTable[LTime.Second() % 10]; 
      memset(ImageShiftCount,0x08,sizeof(ImageShiftCount));
    }
#ifdef ARDUINO
    if(memcmp(&TImage,&NImage,sizeof(TImage)) != 0) 
    {
        NImageCopy();
        showTimeFromNImage();
    }
#endif
}
/***************************/
/*  EEEEEE                 */
/*  EE                  dd */
/*  EE      nnnnn       dd */
/*  EEEE    nn  nn   ddddd */
/*  EE      nn  nn  dd  dd */
/*  EE      nn  nn  dd  dd */
/*  EEEEEE  nn  nn   ddddd */
/*                         */
/***************************/

