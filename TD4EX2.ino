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

// 画面遷移割り当て（display）
#define load_or_new 0
#define rom_input   1
#define run         2
#define load_pvos   3
#define end         4

// 表示内容制御
uint8_t mode = 0; //0:1Hz 1:manual 2:max
uint8_t show = 0;

// 初期設定
uint8_t display = load_or_new;

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

//ROMの読み出し。ROMボードからの出力は、グローバル変数としてrom[]に格納
void rom_init() {
  for (int32_t i = 0; i <= 0b1111; i++) {
    rom[i] = 0b0000;
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

void display_opcode(uint8_t x, uint8_t y, uint8_t addr) {
  char str_op[12];

  lcd.setCursor(x, y);
  switch (rom[addr] >> 4) {
    case ADD_A_Im:
      snprintf(str_op, 11, "ADD  A , %X ", rom[addr] & 0b00001111);
      break;
    case MOV_A_B:
      snprintf(str_op, 11, "MOV  A , B ");
      break;
    case IN_A_Im:
      snprintf(str_op, 11, "IN   A ,[%X]", rom[addr] & 0b00001111);
      break;
    case MOV_A_Im:
      snprintf(str_op, 11, "MOV  A , %X ", rom[addr] & 0b00001111);
      break;
    case MOV_B_A:
      snprintf(str_op, 11, "MOV  B , A ");
      break;
    case ADD_B_Im:
      snprintf(str_op, 11, "ADD  B , %X ", rom[addr] & 0b00001111);
      break;
    case IN_B_Im:
      snprintf(str_op, 11, "IN   B ,[%X]", rom[addr] & 0b00001111);
      break;
    case MOV_B_Im:
      snprintf(str_op, 11, "MOV  B , %X ", rom[addr] & 0b00001111);
      break;
    case PUSH_A:
      snprintf(str_op, 11, "PUSH A     ");
      break;
    case OUT_A_Im:
      snprintf(str_op, 11, "OUT  A ,[%X]", rom[addr] & 0b00001111);
      break;
    case POP_A:
      snprintf(str_op, 11, "POP  A     ");
      break;
    case OUT_Im_A:
      snprintf(str_op, 11, "OUT  %X ,[A]", rom[addr] & 0b00001111);
      break;
    case ADDS:
      snprintf(str_op, 11, "ADDS       ");
      break;
    case JC_Im:
      snprintf(str_op, 11, "JC   %X     ", rom[addr] & 0b00001111);
      break;
    case JNC_Im:
      snprintf(str_op, 11, "JNC  %X     ", rom[addr] & 0b00001111);
      break;
    case JMP_Im:
      snprintf(str_op, 11, "JMP  %X     ", rom[addr] & 0b00001111);
      break;
    default:
      break;
  }
  lcd.print(str_op);
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
  uint8_t arrow = 0;
  while (display == load_or_new) {
    switch(read_LCD_buttons(analogRead(0))) {
      case btnUP:
      case btnDOWN:
        arrow = (++arrow) % 2;
        break;
      case btnSELECT:
        if (arrow == 0) {
            display = 100;
        } else {
            display = rom_input;
        }
      default:
        break;
    }
    delay(100);
    lcd.clear();
    if (arrow == 0) {
      lcd.print("load previous <-");
      lcd.setCursor(0,1);
      lcd.print("create new      ");
    } else {
      lcd.print("load previous   ");
      lcd.setCursor(0,1);
      lcd.print("create new    <-");
    }
  }
}

// display:1
// ----------------
// 0123456789ABCDEF
// 0x00 ***********
// 0 0 0 0 0 0 0 0 
// ----------------
// 0123456789ABCDEF
// RUN?
// YES : NO 
void display_1() {
  uint8_t address = 0x00;
  uint8_t bit = 0;
  while (display == rom_input) {
    switch(read_LCD_buttons(analogRead(0))) {
      case btnUP:
        address = (--address) % 17; //0～15は編集するROMのアドレスに対応、16は実行選択画面
        break;
      case btnDOWN:
        address = (++address) % 17;
        break;
      case btnLEFT:
        bit = (--bit) % 8;
        break;
      case btnRIGHT:
        bit = (++bit) % 8;    
        break;
      case btnSELECT:
        if (address != 16) {
          rom[address] ^= (1 << bit)
        } else {
          display = run;
        }
        break;
      default:
        break;
    } 
    delay(100);
    lcd.clear();
    if (address == 16) {
      lcd.print("RUN the program");
      lcd.setCursor(0,1);
      lcd.print("push select");
    } else {
      lcd.print("0x0");
      lcd.serCursor(3);
      lcd.print(address, HEX);
      lcd.serCursor(5);
      lcd.print(" ")
      display_opcode(5, 0, address);
      for (int i = 0; i < 8; i++) {
        lcd.setCursor(0xE - 2 * i, 1);
        if (rom[address] ^ (1 << i) == 0) {
          lcd.print("0");
        } else {
          lcd.print("1");
        }
      }
      lcd.setCursor(0xE - 2 * bit, 1);
      lcd.cursor();
    }
  }
}

// display:2 
// ----------------
// 0123456789ABCDEF
// m 00 ***********
// A:  B:  S:  c:  
// ----------------
// 0123456789ABCDEF
// m 00 ***********
// score:
// ----------------
// 0123456789ABCDEF
// m 00 ***********
// 0:  1:  2:  3:  
// ----------------
// 0123456789ABCDEF
// m 00 ***********
// 4:  5:  6:  7:  
// ----------------
// 0123456789ABCDEF
// m 00 ***********
// 8:  9:  A:  B:  
// ----------------
// 0123456789ABCDEF
// m 00 ***********
// C:  D:  E:  F:  
void display_2() {
  while (display == run) {
    switch(read_LCD_buttons(analogRead(0))) {
      case btnUP:
        if (show != 0) {
            show--;
        }
        break;
      case btnDOWN:
        if (show != 5) {
            show++;
        }
        break;
      case btnRIGHT:
        mode = (++mode) % 3;
        break;
      case btnLEFT:
        mode = (--mode) % 3;
        break;
      default:
        break;
    }
    if (mode == 1) {
        while(read_LCD_buttons(analogRead(0)) == btnSELECT) {
            delay(100);
        }
    } else if(mode == 0) {
        delay(1000);
    } 
    lcd.setCursor(0,0);
    char str_mode_addr[6];
    snprintf(str_mode_addr, 5, "%u %0X ", mode, rom[reg_pc]);
    lcd.print(str_mode_addr)
    display_opcode(5, 0, reg_pc);
    char str[17];
    switch (show) {
      case 0:
        snprintf(str, 16, "A:%X B:%X S:%X c:%X ", reg_a, reg_b, reg_sp, c_flag);
        break;
      case 1:
        snprintf(str, 16, " score:%lu07  ", score);
        break;
      case 2:
        snprintf(str, 16, "0:%X 1:%X 2:%X 3:%X ", ram[0], ram[1], ram[2], ram[3]);
        break;
      case 3:
        snprintf(str, 16, "4:%X 5:%X 6:%X 7:%X ", ram[4], ram[5], ram[6], ram[7]);
        break;
      case 4:
        snprintf(str, 16, "8:%X 9:%X A:%X B:%X ", ram[8], ram[9], ram[A], ram[B]);
        break;
      case 5:
        snprintf(str, 16, "C:%X D:%X E:%X F:%X ", ram[C], ram[D], ram[E], ram[F]);
        break;
      default:
        break;
    }
    lcd.setCursor(0,1);
    lcd.print(str);
  }
}

void display_4() {
  while (display == end) {
    switch(read_LCD_buttons(analogRead(0))) {
      case btnUP:
        if (show != 0) {
            show--;
        }
        break;
      case btnDOWN:
        if (show != 5) {
            show++;
        }
        break;
      default:
        break;
    }

    lcd.setCursor(0,0);
    char str_mode_addr[6];
    snprintf(str_mode_addr, 5, "* %0X ", mode, rom[reg_pc]);
    lcd.print(str_mode_addr)
    display_opcode(5, 0, reg_pc);
    char str[17];
    switch (show) {
      case 0:
        snprintf(str, 16, "A:%X B:%X S:%X c:%X ", reg_a, reg_b, reg_sp, c_flag);
        break;
      case 1:
        snprintf(str, 16, " score:%lu07  ", score);
        break;
      case 2:
        snprintf(str, 16, "0:%X 1:%X 2:%X 3:%X ", ram[0], ram[1], ram[2], ram[3]);
        break;
      case 3:
        snprintf(str, 16, "4:%X 5:%X 6:%X 7:%X ", ram[4], ram[5], ram[6], ram[7]);
        break;
      case 4:
        snprintf(str, 16, "8:%X 9:%X A:%X B:%X ", ram[8], ram[9], ram[A], ram[B]);
        break;
      case 5:
        snprintf(str, 16, "C:%X D:%X E:%X F:%X ", ram[C], ram[D], ram[E], ram[F]);
        break;
      default:
        break;
    }
    lcd.setCursor(0,1);
    lcd.print(str);
  }
}

void setup() {
  //ROM初期化
  rom_init();
  //RAM初期化
  ram_init();
  //LCD初期化
  lcd.begin(16, 2);
  lcd.setCursor(0,0);
  lcd.print("TD4EX2 Ver1.00");
  delay(1000);

  while (true) {
    switch (display) {
    case load_or_new:
        display_0();
        break;
    case rom_input:
        display_1();
        break;
    case load_pvos:
        //display_2();
        break;
    default:
        break;
    }
  }
  
  //シリアルモニタデバッグ用表示
  //Serial.begin( 9600 );
  //serial_debug();
}

void loop() {
  int8_t opcode = rom[reg_pc] >> 4;
  display_2();
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
  // socore測定
  score++;
  // 終了時の判定
  if (ram[0] > 0b0111) {
    display_4();
 }