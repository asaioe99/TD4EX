#include <LiquidCrystal.h>

//ROMボード接続GPIOの割り当て
#define ROM_IN_0 2   //下位bit
#define ROM_IN_1 3
#define ROM_IN_2 4
#define ROM_IN_3 5   //上位bit

#define ROM_OUT_0 13 //下位bit
#define ROM_OUT_1 12
#define ROM_OUT_2 11
#define ROM_OUT_3 10
#define ROM_OUT_4 9
#define ROM_OUT_5 8
#define ROM_OUT_6 7
#define ROM_OUT_7 6 //上位bit

//命令セット
#define ADD_A_Im 0b0000
#define MOV_A_B  0b0001
#define IN_A_Im  0b0010
#define MOV_A_Im 0b0011
#define MOV_B_A  0b0100
#define ADD_B_Im 0b0101
#define IN_B_Im  0b0110
#define MOV_B_Im 0b0111
#define PUSH_A   0b1000
#define OUT_A_Im 0b1001
#define POP_A    0b1010
#define OUT_Im_A 0b1011
#define ADDS     0b1100
#define JC_Im    0b1101
#define JNC_Im   0b1110
#define JMP_Im   0b1111

// ボタン割り当て
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5
#define ERRORR    6

// 初期設定
uint8_t display = 0;
uint8_t arrow = 0

//ROMRAMレジスタ等
uint8_t rom[16];
uint8_t ram[16]; //HW上では4bitであることに注意
uint8_t c_flag = 0; //将来的には他のフラグも拡張することを考えたい
uint8_t reg_a  = 0b0000; //初期値設定
uint8_t reg_b  = 0b0000;
uint8_t reg_sp = 0b1111; //スタックの向きに注意
uint8_t reg_pc = 0b0000;
//実行ステップ数（スコア）フォーマット指定子に注意（%05u）
uint32_t  score = 0;
//実行速度設定用
uint8_t speed;

// 液晶
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

int read_LCD_buttons(int adc_key_in) {
  if (adc_key_in > 1000) return btnNONE;   //1023, 戻り値5, 5.00V
  if (adc_key_in < 50)   return btnRIGHT;  //0   , 戻り値0, 0V
  if (adc_key_in < 250)  return btnUP;     //144 , 戻り値1, 0.70V
  if (adc_key_in < 450)  return btnDOWN;   //329 , 戻り値2, 1.61V
  if (adc_key_in < 650)  return btnLEFT;   //504 , 戻り値3, 2.47V
  if (adc_key_in < 850)  return btnSELECT; //741 , 戻り値4, 3.62V
 
 /* 全てのifが失敗（通常はこれを返さない）*/
 return ERRORR;                          
}

//引数として渡されたROMのアドレス4bitをROMボードに入力する。
void rom_select(int32_t address) {
  //アドレスを入力する。
  digitalWrite(ROM_IN_0, (address & 0b1000) >> 3);
  digitalWrite(ROM_IN_1, (address & 0b0100) >> 2);
  digitalWrite(ROM_IN_2, (address & 0b0010) >> 1);
  digitalWrite(ROM_IN_3, (address & 0b0001) >> 0);
}
//ROMの読み出し。ROMボードからの出力は、グローバル変数としてrom[]に格納
void rom_read() {
  for (int32_t i = 0b0000; i <= 0b1111; i++) {
    rom_select(i);
    delay(10);//rom読み込み時の信頼性向上のため（効果は不明）
    rom[i] = 0;
    rom[i] += digitalRead(ROM_OUT_0) << 0;
    rom[i] += digitalRead(ROM_OUT_1) << 1;
    rom[i] += digitalRead(ROM_OUT_2) << 2;
    rom[i] += digitalRead(ROM_OUT_3) << 3;
    rom[i] += digitalRead(ROM_OUT_4) << 4;
    rom[i] += digitalRead(ROM_OUT_5) << 5;
    rom[i] += digitalRead(ROM_OUT_6) << 6;
    rom[i] += digitalRead(ROM_OUT_7) << 7;
  }
}
//ramの初期化
void ram_init() {
  for (int32_t i = 0; i <= 0b1111; i++) {
    ram[i] = 0b0000;
  }
}

void add_a_im() {
  int8_t tmp = rom[reg_pc] & 0b00001111;
  reg_a += tmp;
  if (reg_a > 0b1111) {
    c_flag = 1;
    reg_a = reg_a % 16;
  } else {
    c_flag = 0;
  }
  reg_pc = (++reg_pc) % 16;
}
void mov_a_b() {
  int32_t tmp = rom[reg_pc] & 0b00001111;
  reg_a = reg_b + tmp;
  if (reg_a > 0b1111) {
    c_flag = 1;
    reg_a = reg_a % 16;
  } else {
    c_flag = 0;
  }
  reg_pc = (++reg_pc) % 16;
}
void in_a_im() {
  reg_a = ram[rom[reg_pc] & 0b00001111];
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void mov_a_im() {
  reg_a = rom[reg_pc] & 0b00001111;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void mov_b_a() {
  int32_t tmp = rom[reg_pc] & 0b00001111;
  reg_b = reg_a + tmp;
  if (reg_b > 0b1111) {
    c_flag = 1;
    reg_b = reg_b % 16;
  } else {
    c_flag = 0;
  }
  reg_pc = (++reg_pc) % 16;
}
void add_b_im() {
  int32_t tmp = rom[reg_pc] & 0b00001111;
  reg_b += tmp;
  if (reg_b > 0b1111) {
    c_flag = 1;
    reg_b = reg_b % 16;
  } else {
    c_flag = 0;
  }
  reg_pc = (++reg_pc) % 16;
}
void in_b_im() {
  reg_b = ram[rom[reg_pc] & 0b00001111];
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void mov_b_im() {
  reg_b = rom[reg_pc] & 0b00001111;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void push_a() {
  int32_t tmp = rom[reg_pc] & 0b00001111;
  ram[reg_sp] = reg_a + tmp;
  ram[reg_sp] = ram[reg_sp] % 16;
  if (reg_sp > 0) {
    reg_sp--;
  } else {
    reg_sp = 0b1111;
  }
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void out_a_im() {
  ram[reg_a] = rom[reg_pc] & 0b00001111;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void pop_a() {
  int32_t tmp = rom[reg_pc] & 0b00001111;
  reg_sp = (++reg_sp) % 16;
  reg_a = ram[reg_sp] + tmp;
  if (reg_a > 0b1111) {
    c_flag = 1;
    reg_a = reg_a % 16;
  } else {
    c_flag = 0;
  }
  reg_pc = (++reg_pc) % 16;
}
void out_im_a() {
  ram[rom[reg_pc] & 0b00001111] = reg_a;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void adds() {
  int32_t tmp = rom[reg_pc] & 0b00001111;
  ram[(reg_sp + 2) % 16] += ram[(reg_sp + 1) % 16] + tmp;
  ram[(reg_sp + 2) % 16] = ram[(reg_sp + 2) % 16] % 16;
  reg_sp = (++reg_sp) % 16;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void jc_im() {
  if (c_flag == 1) {
    reg_pc = rom[reg_pc] & 0b00001111;
  } else {
    reg_pc = (++reg_pc) % 16;
  }
  c_flag = 0;
}
void jnc_im() {
  if (c_flag == 0) {
    reg_pc = rom[reg_pc] & 0b00001111;
  } else {
    reg_pc = (++reg_pc) % 16;
  }
  c_flag = 0;
}
void jmp_im() {
  reg_pc = rom[reg_pc] & 0b00001111;
  c_flag = 0;
}

void lcd_display(int8_t opcode) {
  char RAM0_5[19];
  char RAM6_B[19];
  char RAMC_Fabsc[21];
  char pOPscr[20];
  char f_ram_cng = 0;

  lcd.setCursor(1, 3);
  switch (opcode) {
    case ADD_A_Im:
      snprintf(pOPscr, 20, "%X:ADD  A ,  %X %05lu", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    case MOV_A_B:
      snprintf(pOPscr, 20, "%X:MOV  A ,  B %05lu", reg_pc, score);
      break;
    case IN_A_Im:
      snprintf(pOPscr, 20, "%X:IN   A ,[0%X]%05lu", reg_pc, rom[reg_pc] & 0b00001111, score);
      f_ram_cng = 1;
      break;
    case MOV_A_Im:
      snprintf(pOPscr, 20, "%X:MOV  A ,  %X %05lu", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    case MOV_B_A:
      snprintf(pOPscr, 20, "%X:MOV  B ,  A %05lu", reg_pc, score);
      break;
    case ADD_B_Im:
      snprintf(pOPscr, 20, "%X:ADD  B ,  %X %05lu", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    case IN_B_Im:
      snprintf(pOPscr, 20, "%X:IN   B ,[0%X]%05lu", reg_pc, rom[reg_pc] & 0b00001111, score);
      f_ram_cng = 1;
      break;
    case MOV_B_Im:
      snprintf(pOPscr, 20, "%X:MOV  B ,  %X %05lu", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    case PUSH_A:
      snprintf(pOPscr, 20, "%X:PUSH A      %05lu", reg_pc, score);
      f_ram_cng = 1;
      break;
    case OUT_A_Im:
      snprintf(pOPscr, 20, "%X:OUT  A ,[0%X]%05lu", reg_pc, rom[reg_pc] & 0b00001111, score);
      f_ram_cng = 1;
      break;
    case POP_A:
      snprintf(pOPscr, 20, "%X:POP  A      %05lu", reg_pc, score);
      f_ram_cng = 1;
      break;
    case OUT_Im_A:
      snprintf(pOPscr, 20, "%X:OUT  %X ,[ A]%05lu", reg_pc, rom[reg_pc] & 0b00001111, score);
      f_ram_cng = 1;
      break;
    case ADDS:
      snprintf(pOPscr, 20, "%X:ADDS        %05lu", reg_pc, score);
      f_ram_cng = 1;
      break;
    case JC_Im:
      snprintf(pOPscr, 20, "%X:JC   %X      %05lu", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    case JNC_Im:
      snprintf(pOPscr, 20, "%X:JNC  %X      %05lu", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    case JMP_Im:
      snprintf(pOPscr, 20, "%X:JMP  %X      %05lu", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    default:
      break;
  }
  lcd.print(pOPscr);

  if (f_ram_cng == 1) {
    snprintf(RAM0_5, 17, "%X1:%X2:%X3:%X4:%X5:%X", ram[0x0], ram[0x1], ram[0x2], ram[0x3], ram[0x4], ram[0x5]);
    snprintf(RAM6_B, 17, "%X7:%X8:%X9:%XA:%XB:%X", ram[0x6], ram[0x7], ram[0x8], ram[0x9], ram[0xA], ram[0xB]);
    lcd.setCursor(2, 0);
    lcd.print(RAM0_5);
    lcd.setCursor(2, 1);
    lcd.print(RAM6_B);
  }

  snprintf(RAMC_Fabsc, 19, "%XD:%XE:%XF:%Xa%Xb%Xs%Xc%X", ram[0xC], ram[0xD], ram[0xE], ram[0xF], reg_a, reg_b, reg_sp, c_flag);
  lcd.setCursor(2, 2);
  lcd.print(RAMC_Fabsc);
}

void serial_debug() {
  for (int32_t i = 0; i < 16; i++) {
    Serial.print(i, HEX);
    Serial.print(":");
    Serial.print((rom[i] & 0b10000000) >> 7, BIN);
    Serial.print((rom[i] & 0b01000000) >> 6, BIN);
    Serial.print((rom[i] & 0b00100000) >> 5, BIN);
    Serial.print((rom[i] & 0b00010000) >> 4, BIN);
    Serial.print((rom[i] & 0b00001000) >> 3, BIN);
    Serial.print((rom[i] & 0b00000100) >> 2, BIN);
    Serial.print((rom[i] & 0b00000010) >> 1, BIN);
    Serial.println(rom[i] & 0b00000001, BIN);
  }
}

// display:0 arrow:0
// ----------------
// 0123456789ABCDEF
// load previous <-
// create new    
// display:0 arrow:0
// ----------------
// 0123456789ABCDEF
// load previous
// create new    <-
void display_0() {
  while (display == 0) {
    switch(read_LCD_buttons(analogRead(0))) {
      case btnUP:
      case btnDOWN:
        arrow = (++arrow) % 2;
        break;
      case btnSELECT:
        display++;
      default:
        break;
    }
    if (arrow % 2 == 0) {
      lcd.setCursor(0,0);
      lcd.print("load previous <-");
      lcd.setCursor(0,1);
      lcd.print("create new      ");
    } else {
      lcd.setCursor(0,0);
      lcd.print("load previous   ");
      lcd.setCursor(0,1);
      lcd.print("create new    <-");
    }
    delay(100);
  }
}

// display:1 arrow:0
// ----------------
// 0123456789ABCDEF
// 0x00 ***********
// 0 0 0 0 0 0 0 0 
// ----------------
// 0123456789ABCDEF
// RUN?
// YES : NO 
void display_1() {
  uint8_t address = 0b0000;
  while (display == 1) {
    switch(read_LCD_buttons(analogRead(0))) {
      case btnUP:
        arrow = (--address) % 17;
        break;
      case btnDOWN:
        arrow = (++address) % 17;
        break;
      case btnSELECT:
        display++;
      default:
        break;
    } 
  }
}

void setup() {

  //LCD初期化
  lcd.begin(16, 2);
  lcd.setCursor(0,0);            //カーソルの移動
  lcd.print("TD4EX2 Ver1.00");     //文字の出力
  delay(1000);


  // 初期設定
  uint8_t lcd_key;
  uint8_t display = 0;
  uint8_t arrow = 0

// ボタン割り当て
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5
#define ERRORR    6


  if (arrow == 0) {
    display_1_new();
  }




  //ROM読み込み
  rom_read();
  //RAM初期化
  ram_init();
  //シリアルモニタデバッグ用表示
  //Serial.begin( 9600 );
  //serial_debug();
}

void loop() {
  rom_select(reg_pc); //ROMボードを点灯させるための処理（内部的には不要）
  int8_t opcode = rom[reg_pc] >> 4;
  lcd_display(opcode);
  switch (opcode) {
    case ADD_A_Im:
      add_a_im();
      break;
    case MOV_A_B:
      mov_a_b();
      break;
    case IN_A_Im:
      in_a_im();
      break;
    case MOV_A_Im:
      mov_a_im();
      break;
    case MOV_B_A:
      mov_b_a();
      break;
    case ADD_B_Im:
      add_b_im();
      break;
    case IN_B_Im:
      in_b_im();
      break;
    case MOV_B_Im:
      mov_b_im();
      break;
    case PUSH_A:
      push_a();
      break;
    case OUT_A_Im:
      out_a_im();
      break;
    case POP_A:
      pop_a();
      break;
    case OUT_Im_A:
      out_im_a();
      break;
    case ADDS:
      adds();
      break;
    case JC_Im:
      jc_im();
      break;
    case JNC_Im:
      jnc_im();
      break;
    case JMP_Im:
      jmp_im();
      break;
    default:
      break;
  }
  score++;
  if (speed == 1) {
    delay(1000);
  }
  if (ram[0] > 0b0111) {
    lcd_display(opcode);
    lcd.setCursor(19, 0);
    lcd.print("*");
    for (;;) {
      delay(10000);
    }
 }
