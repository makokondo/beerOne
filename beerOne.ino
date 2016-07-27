//This example implements a simple sliding On/Off button. The example
// demonstrates drawing and touch operations.
//
//Thanks to Adafruit forums member Asteroid for the original sketch!
//
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_STMPE610.h>

#include <Control.h>
#include <MsTimer2.h>
char nlog[200]; //debug buffer
void Log(char *buf);
#define INTERVAL_MSEC_NORMAL (2*60*1000L)
#define INTERVAL_MSEC_FAST (2*1000L)
/********************************************
  Cooler Heater定義
*********************************************/
#define ENABLE_COOLER
#define HEATER_PIN 22 //ヒーター駆動 ON:1 OFF:0
#define COOLER_PIN 23 //クーラー駆動 ON:1 OFF:0
#define CHOTFAN_PIN 24 //クーラーホット側ファン駆動 ON:1 OFF:0
#define PERFAN_PIN 25 //クーラーペルチェ側ファン駆動 ON:1 OFF:0
#define CH_DELAY_MSEC 30000L //駆動遅延時間(msec) 温度差が小さいときは前回駆動変化後、この時間経過するまで駆動しない
#define COOLFAN_INTERVAL_MSEC 180000L //3分クーラーホット側ファン停止インターバル
/********************************************
  リファレンス温度センサLM35定義
*********************************************/
#define PIN_LM35GND 28 //このピンから電源GNDを取っているだけ
#define PIN_LM35TEMP 15 //Analog15
#define ENABLE_LM35

/********************************************
  温湿度センサ AM2320
*********************************************/
#include <DHT.h>


#define DHTPIN 29     // what digital pin we're connected to
#define PIN_DHTGND 28 //このピンから電源GNDを取っているだけ
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor
#define ENABLE_DHT

/********************************************
  信号
*********************************************/
struct stSignal {
    char Temp,Ref;
    char Hum;
};
typedef struct stSignal TSignal;
#define SIGNAL_SIZE 1600 //Signals[]配列確保サイズ
#define TimerInterval_msec__  16 //タイマーイベント間隔

class TBeerOne {
  public:
  long msec,last_msec,last_dht_msec,last_eval_msec; 
  bool isTemp;
  int Temperature,Humidity;
#ifdef ENABLE_DHT
  DHT *dht;// Initialize DHT sensor.
#endif //ENABLE_DHT
  long last_ch_msec; //前回クーラーヒータ駆動変化時間
  long last_cooloff_msec; //前回クーラー駆動OFF時間記録
  long SignalStartMSEC; //以下のSignals[0]の記録時刻
  long SignalIntervalMSEC; //記録間隔
  long EvalIntervalMSEC; //温度評価間隔
  int SignalCount; //Signals有効信号数
  TSignal Signals[SIGNAL_SIZE]; //信号記録配列
  int GraphStart; //Signals[]の描画開始インデックス
  TBeerOne();
  void Timer();
  void Clock();
  void AddSignal(int temp,int hum,int ref);
  void DrawGraphLines(int color);
  void DrawGraph(); //グラフ初期描画
  void DrawGraphScaleX(int color); //X軸スケール描画
  int GetX(int x); //グラフプロットX座標に変換
  int GetY(int y); //グラフプロットY座標に変換
  int GetY(float y); //グラフプロットY座標に変換 浮動小数版
  int GraphWidthX(); //X軸グラフ描画点数
  int GetIntervalPixel(); //X軸グラフ１画素あたりのSignals要素数（間引き数）
};
TBeerOne be; //静的アプリケーション実行クラス

/********************************************
  画面初期化
*********************************************/
TButton *CurrentTemp,*ButtonRed,*ButtonBlue,*SettingTemp,*RefTemp,*RefHum,*Radio2Sec,*Radio2Min;
TControl *GraphTemp,*LabelTimer; //温度グラフコントロール
void setupForm(void) //画面初期化
{
  int x=1; int y=10;
  TButton *b;
  b=new TButton("+"); ButtonRed=b; b->Width=26; b->X=x+10; b->Y=y; b->Color=ILI9341_PINK; b->BorderColor=ILI9341_PINK;b->OnClick=OnClickPlus; b->Draw();

  y+=b->Height;
  b=new TButton(10); SettingTemp=b; b->Tag=30; sprintf(b->Text,"%d",b->Tag); b->Width=40; b->X=x; b->Y=y; b->Color=ILI9341_DARKGREEN; b->Draw();
  b=new TButton(10); RefTemp=b; b->Width=60; b->Width=40; b->X=x+42; b->Y=y; b->FontColor=ILI9341_LIGHTGREY; b->Draw();
  b->DrawCaption(2,-8,"Temp"); //(X,Y)からオフセットした位置にcaption文字列を描画

  y+=b->Height;
  b=new TButton("-"); ButtonBlue=b; b->Width=26; b->X=x+10; b->Y=y; b->Color=ILI9341_DARKCYAN; b->BorderColor=ILI9341_DARKCYAN;b->OnClick=OnClickMinus; b->Draw();

  y+=10+b->Height+2;
  b=new TButton(10); CurrentTemp=b; b->Width=40; b->X=x; b->Y=y; b->Color=ILI9341_DARKGREEN; b->FontColor=ILI9341_LIGHTGREY; b->Draw();
  b->DrawCaption(0,-8,"Temp2");
  b=new TButton(10); RefHum=b; b->Width=40; b->X=x+42; b->Y=y; b->FontColor=ILI9341_LIGHTGREY; b->OnClick=OnClickMinus; b->Draw();
  b->DrawCaption(0,-8,"Hum2");

  y+=b->Height+12;
  b=new TButton(10); strcpy(b->Text,"o 2M"); Radio2Min=b; b->FontSize=1; b->Width=40; b->Height=22; b->X=x; b->Y=y; b->Color=ILI9341_DARKCYAN; b->BorderColor=ILI9341_DARKCYAN; b->OnClick=OnClick2M; b->Draw();
  b->DrawCaption(0,-8,"sampling time");
  b=new TButton(10); strcpy(b->Text,"  2S"); Radio2Sec=b; b->FontSize=1; b->Width=40; b->Height=22; b->X=x+42; b->Y=y; b->Color=ILI9341_PINK; b->BorderColor=ILI9341_PINK; b->OnClick=OnClick2S; b->Draw();
  
  y=220;
  TControl *c=new TControl(12); LabelTimer=c; strcpy(c->Text,"  D  H  M"); c->Width=120; c->X=x; c->Y=y; c->BorderColor=c->Color; c->FontColor=ILI9341_LIGHTGREY; c->Draw();

  c=new TControl(""); GraphTemp=c; c->Width=220; c->Height=200; c->X=100; c->Y=1; c->FontColor=ILI9341_LIGHTGREY; c->Draw(); //温度グラフコントロール
}

/********************************************
  画面イベント
*********************************************/
void OnClickPlus(TControl *Self,int ev)
{
  SettingTemp->Tag++; sprintf(SettingTemp->Text,"%d",SettingTemp->Tag);
  SettingTemp->Draw();
}
void OnClickMinus(TControl *Self,int ev)
{
  SettingTemp->Tag--; sprintf(SettingTemp->Text,"%d",SettingTemp->Tag);
  SettingTemp->Draw();
}
void OnClick2M(TControl *Self,int ev)
{
  be.SignalIntervalMSEC=INTERVAL_MSEC_NORMAL; //記録間隔
  strcpy(Radio2Sec->Text,"  2S"); Radio2Sec->Draw();
  strcpy(Radio2Min->Text,"o 2M"); Radio2Min->Draw();
  Log("OnClick2M");
}
void OnClick2S(TControl *Self,int ev)
{
  be.SignalIntervalMSEC=INTERVAL_MSEC_FAST; //記録間隔
  strcpy(Radio2Sec->Text,"o 2S"); Radio2Sec->Draw();
  strcpy(Radio2Min->Text,"  2M"); Radio2Min->Draw();
  Log("OnClick2S");
}
int TBeerOne::GetX(int x) //グラフプロットX座標に変換
{ TControl *c=GraphTemp;
  return c->X+x;
}
int TBeerOne::GetY(int y) //グラフプロットY座標に変換
{ TControl *c=GraphTemp;
  return c->Y+c->Height-2*y;
}
int TBeerOne::GetY(float y) //グラフプロットY座標に変換 浮動小数版
{ TControl *c=GraphTemp;
  return c->Y+c->Height-2*y;
}
int TBeerOne::GraphWidthX() //X軸グラフ描画点数
{
  return GraphTemp->Width-3;
}
int TBeerOne::GetIntervalPixel( ) //X軸グラフ１画素あたりのSignals要素数（間引き数）
{
  int x_count=GraphWidthX(); //X方向描画ポイント可能数
  int IntervalPixel=1+SignalCount/x_count;
  return IntervalPixel;
}
void TBeerOne::DrawGraph()
{ 
  GraphTemp->Draw(); //基本矩形部の描画
  for(int t=10;t<100;t+=10) { //Y軸温度１０度毎スケール描画
    gcm->tft->drawLine(GetX(0),GetY(t),GetX(2),GetY(t),ILI9341_DARKGREY);
    if( (t==10)||(t==30)||(t==60) ) {
      gcm->tft->setCursor(GetX(-14) , GetY(t+4));
      gcm->tft->setTextColor(GraphTemp->FontColor);
      gcm->tft->setTextSize(1);
      sprintf(nlog,"%d",t); gcm->tft->println(nlog);

    }
  }
  gcm->tft->fillRect(GetX(0), GetY(0), GraphTemp->Width, 24,ILI9341_BLACK ); //X軸スケール部クリア
  DrawGraphScaleX(ILI9341_DARKGREY); //X軸スケール描画
  DrawGraphLines(GraphTemp->FontColor); //データライン描画
}

void TBeerOne::DrawGraphScaleX(int color) //X軸スケール描画
{ TSignal *sig; 
#define SCALE_INTERVAL_X 40 //x軸スケール間隔
  bool upper=true;
  int x_count=GraphWidthX(); //X方向描画ポイント可能数
  int IntervalPixel=GetIntervalPixel();
  sprintf(nlog,"ip%d x_count%d SignalStartMSEC%ld\n",IntervalPixel,x_count,SignalStartMSEC); Log(nlog);
  for(int x=40;x<GraphWidthX();x+=SCALE_INTERVAL_X) { //X軸スケール描画
      gcm->tft->drawLine(GetX(x),GetY(0),GetX(x),GetY(-3),ILI9341_DARKGREY); //縦引き出し線
      long xsec=SignalStartMSEC/1000L+(GraphStart+IntervalPixel*(long)x)*(SignalIntervalMSEC/1000); //x位置の時刻算出
      int sec=xsec%60;
      int xmin=xsec/60;
      int m=xmin%60;
      int xhour=xmin/60;
      int hour=xhour%24;
      int xday=xhour/24;
      int Y=GetY(-3);
      if( upper ) {
        upper=false;
      } else {
        upper=true;
        Y=GetY(-3-5);
      }
      gcm->tft->setCursor(GetX(x-26) , Y);
      gcm->tft->setTextColor(color);
      gcm->tft->setTextSize(1);
      sprintf(nlog,"%d/%02d:%02d",xday,hour,m); gcm->tft->println(nlog);
  }
}

void TBeerOne::DrawGraphLines(int color)
{ TSignal *sig; 
  int x_count=GraphWidthX(); //X方向描画ポイント可能数
  int IntervalPixel=GetIntervalPixel(); //間引き間隔
  
  for(int k=0;k<x_count;k++) {
    int i=GraphStart+k*IntervalPixel;
    if( SignalCount<=i ) break;
    sig=&Signals[i];
    float ref,tt,hh;
      ref=sig->Ref;
      tt=sig->Temp;
      hh=sig->Hum;
    gcm->tft->drawPixel(GetX(2+k),GetY(tt),ILI9341_RED);
    gcm->tft->drawPixel(GetX(2+k),GetY(hh),ILI9341_CYAN);
    gcm->tft->drawPixel(GetX(2+k)+1,GetY(ref),color);
  }
}
/********************************************
  信号追加
*********************************************/
void TBeerOne::AddSignal(int temp,int hum,int ref)
{ TSignal *sig;
  if( SignalCount==0 ) { //最初なら
    SignalStartMSEC=msec; //以下のSignals[0]の記録時刻
  }  
  int ip0=GetIntervalPixel(); //X軸グラフ１画素あたりのSignals要素数（間引き数）
  bool isFullDraw=false; //フルグラフ描画必要フラグ
  if( SIGNAL_SIZE<=SignalCount ) { //信号配列満杯なら
#define DATA_DROP_COUNT 10 //信号配列が満杯時に先に入れたものを落す数 
    memcpy(&Signals[0],&Signals[DATA_DROP_COUNT],sizeof(TSignal)*(SIGNAL_SIZE-DATA_DROP_COUNT-1)); //先頭を突き落とし
    SignalStartMSEC+=(SignalIntervalMSEC*DATA_DROP_COUNT); //Signals[0]の記録時刻
    SignalCount-=DATA_DROP_COUNT-1;
    isFullDraw=true;
  } else {
    SignalCount++;
  }
  sig=&Signals[SignalCount-1];
  sig->Temp=temp; sig->Hum=hum; sig->Ref=ref;
  int ip=GetIntervalPixel(); //X軸グラフ１画素あたりのSignals要素数（間引き数）
  if( (isFullDraw)||(ip0!=ip) ) { //信号配列満杯処理かスケール時間変更あれば
    DrawGraph(); //フルグラフ描画
  } else {
    DrawGraphLines(GraphTemp->FontColor); //追加線だけ再描画
  }
}

TBeerOne::TBeerOne() 
{
  msec=last_msec=last_dht_msec=last_eval_msec=0;
  last_ch_msec=0; //前回クーラーヒータ駆動変化時間
  last_cooloff_msec=0; //前回クーラー駆動OFF時間
  Temperature=0; Humidity=0;
  SignalCount=0;  
  SignalStartMSEC=0;
  SignalIntervalMSEC=INTERVAL_MSEC_NORMAL;
  EvalIntervalMSEC=2000; //温度評価間隔
  GraphStart=0; //Signals[]の描画開始インデックス
#ifdef ENABLE_COOLER
    digitalWrite(HEATER_PIN,LOW);  pinMode(HEATER_PIN, OUTPUT); //ヒーター駆動 ON:1 OFF:0
    digitalWrite(COOLER_PIN,LOW);  pinMode(COOLER_PIN, OUTPUT); //クーラー駆動 ON:1 OFF:0
    digitalWrite(CHOTFAN_PIN,LOW);  pinMode(CHOTFAN_PIN, OUTPUT); //クーラーホット側ファン駆動 ON:1 OFF:0
    digitalWrite(PERFAN_PIN,LOW);  pinMode(PERFAN_PIN, OUTPUT); //クーラーペルチェ側ファン駆動 ON:1 OFF:0
#endif //ENABLE_COOLER

#ifdef ENABLE_DHT
  pinMode(PIN_DHTGND, OUTPUT); //このピンから電源GNDを取っているだけ
  digitalWrite(PIN_DHTGND,LOW);
  dht=new DHT(DHTPIN, DHTTYPE);// Initialize DHT sensor.
  dht->begin(); //温度センサ開始
#endif //ENABLE_DHT

}
void TBeerOne::Timer()
{
  msec += TimerInterval_msec__;
}
void TBeerOne::Clock()
{
  //時計表示
  if( last_msec+60L*1000L<msec ) { //1分毎に再描画
    last_msec=msec; 
    int d,h,m,s;
    SecToDayHourMin(msec/1000,&d,&h,&m,&s);//通算秒を日、時間、分、秒に変換
    sprintf(LabelTimer->Text,"%2dD%2dH%2dM",d,h,m);
    LabelTimer->Draw();
  }
  //温度サンプリング
  if( last_eval_msec+EvalIntervalMSEC<msec ) { //温度評価間隔
    last_eval_msec=msec;
#ifdef ENABLE_DHT
    if( isTemp ) { //それぞれ250ms潜るので交互に読み出す
      isTemp=false;
      Temperature = dht->readTemperature(); // Read temperature as Celsius (the default)
      sprintf(CurrentTemp->Text,"%d",Temperature); //温湿度の表示更新
      CurrentTemp->Draw();
    } else {
      isTemp=true;
      Humidity= dht->readHumidity();
      sprintf(RefHum->Text,"%d",Humidity); //温湿度の表示更新
      RefHum->Draw();
    }
#endif //ENABLE_DHT

#ifdef ENABLE_LM35
    int last=RefTemp->Tag;
    // アナログ分解能10bit 5V/1024=0.0048V/LSB   LM35は10mV/℃だから分解能＝0.5℃
    //28℃の時、28+30=58    0.0048Vx58=0.278V
    //温度＝AD値*0.0048V *100=AD値*48/100
    RefTemp->Tag=3+(analogRead(PIN_LM35TEMP)*48)/100; //AD値からこの値を引くと摂氏
    if( last!=RefTemp->Tag ) {
        sprintf(nlog,"temp %d\n",RefTemp->Tag); Log(nlog); 
    }
 #ifdef ENABLE_COOLER //冷却駆動するなら
    int dt=RefTemp->Tag - SettingTemp->Tag;
    if( 1<=dt ) { //設定より現在温度が高いなら
      if( digitalRead(HEATER_PIN)==HIGH ) { //ヒーターONしていたら
        digitalWrite(HEATER_PIN,LOW); //ヒーター停止
        last_ch_msec=msec; //前回クーラーヒータ駆動変化時間記録
        sprintf(nlog,"%d-%d HEATER OFF-\n",RefTemp->Tag,SettingTemp->Tag); Log(nlog); 
      } else if( (last_ch_msec+CH_DELAY_MSEC<msec)||(2<=dt) ) { //時間経過するか2度以上差があれば
        digitalWrite(COOLER_PIN,HIGH); //クーラー駆動 ON:1 OFF:0
        last_ch_msec=msec; //前回クーラーヒータ駆動変化時間記録
        digitalWrite(CHOTFAN_PIN,HIGH); //クーラーホット側ファン駆動 ON:1 OFF:0
        last_cooloff_msec=msec; //前回クーラー駆動時間記録
        digitalWrite(PERFAN_PIN,HIGH); //クーラーペルチェ側ファン駆動 ON:1 OFF:0
        sprintf(nlog,"%d-%d COOLER ON\n",RefTemp->Tag,SettingTemp->Tag); Log(nlog); 
      }
    } else if( dt<=-1 ) { //設定より現在温度が低いなら
      if( digitalRead(COOLER_PIN)==HIGH ) { //クーラーONしていたら
        digitalWrite(COOLER_PIN,LOW); //クーラー停止
        last_ch_msec=msec; //前回クーラーヒータ駆動変化時間記録
        last_cooloff_msec=msec; //前回クーラー駆動時間記録
        sprintf(nlog,"%d-%d COOLER OFF-\n",RefTemp->Tag,SettingTemp->Tag); Log(nlog); 
      } else if( (last_ch_msec+CH_DELAY_MSEC<msec)||(dt<=-2) ) { //時間経過するか2度以上差があれば
        digitalWrite(HEATER_PIN,HIGH); //ヒーター駆動 ON:1 OFF:0
        last_ch_msec=msec; //前回クーラーヒータ駆動変化時間記録
        digitalWrite(PERFAN_PIN,HIGH); //クーラーペルチェ側ファン駆動 ON:1 OFF:0
        sprintf(nlog,"%d-%d HEATER ON\n",RefTemp->Tag,SettingTemp->Tag); Log(nlog); 
      }
    } else { //設定温度と一致なら
      if( digitalRead(HEATER_PIN)==HIGH ) { //ヒーターONしていたら
        digitalWrite(HEATER_PIN,LOW); //ヒーター停止
        last_ch_msec=msec; //前回クーラーヒータ駆動変化時間記録
        sprintf(nlog,"%d-%d HEATER OFF\n",RefTemp->Tag,SettingTemp->Tag); Log(nlog); 
      }
      if( digitalRead(COOLER_PIN)==HIGH ) { //クーラーONしていたら
        digitalWrite(COOLER_PIN,LOW); //クーラー停止
        last_ch_msec=msec; //前回クーラーヒータ駆動変化時間記録
        last_cooloff_msec=msec; //前回クーラー駆動時間記録
        sprintf(nlog,"%d-%d COOLER OFF\n",RefTemp->Tag,SettingTemp->Tag); Log(nlog); 
      }
    }
    if( (0<last_cooloff_msec)&&(last_cooloff_msec+COOLFAN_INTERVAL_MSEC<msec)&&(LOW==digitalRead(COOLER_PIN)) ) { //クーラー駆動変化時間が有効で、インターバル以上経過し、現在クーラー駆動していないなら
        digitalWrite(CHOTFAN_PIN,LOW); //クーラーホット側ファン駆動 ON:1 OFF:0
        sprintf(nlog,"FAN OFF\n"); Log(nlog); 
    }
    //クーラーヒータ状態の表示
    if( digitalRead(COOLER_PIN)==HIGH ) { //クーラーONしていたら
      if( digitalRead(HEATER_PIN)==HIGH ) { //ヒーターONしていたら
        RefTemp->Color=ILI9341_PURPLE; //毒状態
      } else {
        RefTemp->Color=ILI9341_BLUE; //青
      }
    } else if( digitalRead(HEATER_PIN)==HIGH ) { //ヒーターONしていたら
      RefTemp->Color=ILI9341_RED; //赤
    } else { //クーラーもヒーターもONしていなければ
      RefTemp->Color=ILI9341_DARKGREEN; //緑
    }
    if( digitalRead(CHOTFAN_PIN)==HIGH ) { //クーラーホット側ファンONしていたら
      RefTemp->BorderColor=ILI9341_YELLOW;
    } else {
      RefTemp->BorderColor=ILI9341_CYAN;
    }
#endif //ENABLE_COOLER
    sprintf(RefTemp->Text,"%d",RefTemp->Tag); //温湿度の表示更新
    RefTemp->Draw();
    if( last_dht_msec+SignalIntervalMSEC<msec ) {
      last_dht_msec=msec;
      AddSignal(Temperature,Humidity,RefTemp->Tag);
    }
#endif //ENABLE_LM35
  }

}
void Log(char *buf)
{ char tbuf[20];
  sprintf(tbuf,"%ld>",be.msec); Serial.print(tbuf);
  Serial.print(buf);
}
/********************************************
  初期化
*********************************************/
void setup(void)
{
  Serial.begin(38400);
  MsTimer2::set(TimerInterval_msec__, TimerFunction);

  gcm=new TControlManager(&be.msec); //グラフィックスコントロール初期化

  if (!gcm->ts_state) { 
    Serial.println("Unable to start touchscreen.");
  } 
  else { 
    Serial.println("Touchscreen started."); 
  }

  setupForm(); //画面初期化
  be.DrawGraph(); //画面グラフ部初期描画


  MsTimer2::start();

}
void TimerFunction()
{
  be.Timer();
}

/********************************************
  ループ処理
*********************************************/
void loop()
{
  gcm->Clock(); //画面イベント更新
  delay(100);
  be.Clock();
}



