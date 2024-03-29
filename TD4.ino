#include <LiquidCrystal.h>
#include <EEPROM.h>

#define ADD_A_Im 0b0000
#define MOV_A_B 0b0001
#define IN_A 0b0010
#define MOV_A_Im 0b0011
#define MOV_B_A 0b0100
#define ADD_B_Im 0b0101
#define IN_B 0b0110
#define MOV_B_Im 0b0111
#define OUT_Im1 0b1000
#define OUT_B 0b1001
#define OUT_Im2 0b1010
#define OUT_Im3 0b1011
#define JMP_B1 0b1100
#define JMP_B2 0b1101
#define JNC_Im 0b1110
#define JMP_Im 0b1111

// ボタン割り当て
#define btnRIGHT 0
#define btnUP 1
#define btnDOWN 2
#define btnLEFT 3
#define btnSELECT 4
#define btnNONE 5
#define ERRORR 6

// 画面遷移割り当て（display）
#define load_or_new 0
#define rom_input 1
#define run_pgm 2
#define load_pvos 3
#define end_emu 4
#define mode_select 5

// dekay()時間
#define ttn 150

// 表示内容制御
uint8_t mode = 0;  //0:100Hz 1:1Hz 2:fast
uint8_t show = 1;

// 命令コード表示用
char str_op[16][12];

// 初期設定
uint8_t display = mode_select;
uint8_t num_save;

//ROMレジスタ等
uint8_t rom[16];
uint8_t c_flag = 0;      //将来的には他のフラグも拡張することを考えたい
uint8_t reg_a = 0b0000;  //初期値設定
uint8_t reg_b = 0b0000;
uint8_t reg_out = 0b0000;
uint8_t reg_pc = 0b0000;
//実行ステップ数（スコア）フォーマット指定子に注意（%lu）
uint32_t score = 0;

// 液晶
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// ボタン判定
int read_LCD_buttons(int adc_key_in) {
  if (adc_key_in > 1000) return btnNONE;   //1023, 戻り値5, 5.00V
  if (adc_key_in < 50) return btnRIGHT;    //0   , 戻り値0, 0V
  if (adc_key_in < 250) return btnUP;      //144 , 戻り値1, 0.70V
  if (adc_key_in < 450) return btnDOWN;    //329 , 戻り値2, 1.61V
  if (adc_key_in < 650) return btnLEFT;    //504 , 戻り値3, 2.47V
  if (adc_key_in < 850) return btnSELECT;  //741 , 戻り値4, 3.62V

  // 全てのifが失敗（通常はこれを返さない）
  return ERRORR;
}

// EEPROM内のデータ構造(未実装機能多数、競技者の情報を保存し調査可能にしたい)
//n+00 ROM[0]
//n+0f ROM[F]
//n+10 score & b1111 0000 0000 0000
//n+11 score & b0000 1111 0000 0000
//n+12 score & b0000 0000 1111 0000
//n+13 score & b0000 0000 0000 1111
//n+14 RSV
//n+1F RSV

// ROMをEEPROMから読み出し
void rom_load(uint8_t num) {
  for (uint8_t i = 0x00; i < 0x10; i++) {
    rom[i] = EEPROM.read(num * 0x20 + i);
  }
}

// ROMをEEPROMに書き込み
void rom_save(uint8_t num) {
  for (uint8_t i = 0x00; i < 0x10; i++) {
    EEPROM.write(num * 0x20 + i, rom[i]);
  }
}

//ROMの読み出し。ROMボードからの出力は、グローバル変数としてrom[]に格納
void rom_init() {
  for (int32_t i = 0; i <= 0b1111; i++) {
    rom[i] = 0b00000000;
  }
}

void add_a_im() {
  uint8_t tmp = rom[reg_pc] & 0b00001111;
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
  uint8_t tmp = rom[reg_pc] & 0b00001111;
  reg_a = reg_b + tmp;
  if (reg_a > 0b1111) {
    c_flag = 1;
    reg_a = reg_a % 16;
  } else {
    c_flag = 0;
  }
  reg_pc = (++reg_pc) % 16;
}
void in_a() {
  uint8_t tmp = rom[reg_pc] & 0b00001111;
  reg_a = 0b0000;
  reg_a += digitalRead(11) << 0;
  reg_a += digitalRead(12) << 1;
  reg_a += digitalRead(13) << 2;
  reg_a += tmp;
  if (reg_a > 0b1111) {
    c_flag = 1;
    reg_a = reg_a % 16;
  } else {
    c_flag = 0;
  }
  reg_pc = (++reg_pc) % 16;
}
void mov_a_im() {
  reg_a = rom[reg_pc] & 0b00001111;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void mov_b_a() {
  uint8_t tmp = rom[reg_pc] & 0b00001111;
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
  uint8_t tmp = rom[reg_pc] & 0b00001111;
  reg_b += tmp;
  if (reg_b > 0b1111) {
    c_flag = 1;
    reg_b = reg_b % 16;
  } else {
    c_flag = 0;
  }
  reg_pc = (++reg_pc) % 16;
}
void in_b() {
  uint8_t tmp = rom[reg_pc] & 0b00001111;
  reg_b = 0b0000;
  reg_b += digitalRead(11) << 0;
  reg_b += digitalRead(12) << 1;
  reg_b += digitalRead(13) << 2;
  reg_b += tmp;
  if (reg_b > 0b1111) {
    c_flag = 1;
    reg_b = reg_b % 16;
  } else {
    c_flag = 0;
  }
  reg_pc = (++reg_pc) % 16;
}
void mov_b_im() {
  reg_b = rom[reg_pc] & 0b00001111;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void out_im1() {
  uint8_t tmp = rom[reg_pc] & 0b00001111;
  reg_out = tmp;
  digitalWrite(0, (reg_out & 0b0001) >> 0);  //下位
  digitalWrite(1, (reg_out & 0b0010) >> 1);
  digitalWrite(2, (reg_out & 0b0100) >> 2);
  digitalWrite(3, (reg_out & 0b1000) >> 3);  //上位
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void out_b() {
  uint8_t tmp = rom[reg_pc] & 0b00001111;
  tmp = reg_b + tmp;

  if (tmp > 0b1111) {
    c_flag = 1;
    tmp = tmp % 16;
  } else {
    c_flag = 0;
  }
  reg_out = tmp;

  digitalWrite(0, (reg_out & 0b0001) >> 0);  //下位
  digitalWrite(1, (reg_out & 0b0010) >> 1);
  digitalWrite(2, (reg_out & 0b0100) >> 2);
  digitalWrite(3, (reg_out & 0b1000) >> 3);  //上位

  reg_pc = (++reg_pc) % 16;
}
void out_im2() {
  uint8_t tmp = rom[reg_pc] & 0b00001111;
  reg_out = tmp;
  digitalWrite(0, (reg_out & 0b0001) >> 0);  //下位
  digitalWrite(1, (reg_out & 0b0010) >> 1);
  digitalWrite(2, (reg_out & 0b0100) >> 2);
  digitalWrite(3, (reg_out & 0b1000) >> 3);  //上位
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void out_im3() {
  uint8_t tmp = rom[reg_pc] & 0b00001111;
  reg_out = tmp;
  digitalWrite(0, (reg_out & 0b0001) >> 0);  //下位
  digitalWrite(1, (reg_out & 0b0010) >> 1);
  digitalWrite(2, (reg_out & 0b0100) >> 2);
  digitalWrite(3, (reg_out & 0b1000) >> 3);  //上位
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void jmp_b1() {
  reg_pc = reg_b;
  c_flag = 0;
}
void jmp_b2() {
  reg_pc = reg_b;
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

void init_display_opcode() {
  for (int i = 0; i < 16; i++) {
    switch (rom[i] >> 4) {
      case ADD_A_Im:
        snprintf(&str_op[i][0], 12, "ADD  A , %X ", rom[i] & 0b00001111);
        break;
      case MOV_A_B:
        snprintf(&str_op[i][0], 12, "MOV  A , B ");
        break;
      case IN_A:
        snprintf(&str_op[i][0], 12, "IN   A     ");
        break;
      case MOV_A_Im:
        snprintf(&str_op[i][0], 12, "MOV  A , %X ", rom[i] & 0b00001111);
        break;
      case MOV_B_A:
        snprintf(&str_op[i][0], 12, "MOV  B , A ");
        break;
      case ADD_B_Im:
        snprintf(&str_op[i][0], 12, "ADD  B , %X ", rom[i] & 0b00001111);
        break;
      case IN_B:
        snprintf(&str_op[i][0], 12, "IN   B     ");
        break;
      case MOV_B_Im:
        snprintf(&str_op[i][0], 12, "MOV  B , %X ", rom[i] & 0b00001111);
        break;
      case OUT_Im1:
        snprintf(&str_op[i][0], 12, "OUT  %X     ", rom[i] & 0b00001111);
        break;
      case OUT_B:
        snprintf(&str_op[i][0], 12, "OUT  B     ");
        break;
      case OUT_Im2:
        snprintf(&str_op[i][0], 12, "OUT  %X     ", rom[i] & 0b00001111);
        break;
      case OUT_Im3:
        snprintf(&str_op[i][0], 12, "OUT  %X     ", rom[i] & 0b00001111);
        break;
      case JMP_B1:
        snprintf(&str_op[i][0], 12, "JMP  B     ");
        break;
      case JMP_B2:
        snprintf(&str_op[i][0], 12, "JMP  B     ");
        break;
      case JNC_Im:
        snprintf(&str_op[i][0], 12, "JNC  %X     ", rom[i] & 0b00001111);
        break;
      case JMP_Im:
        snprintf(&str_op[i][0], 12, "JMP  %X     ", rom[i] & 0b00001111);
        break;
      default:
        break;
    }
  }
}

void display_opcode_fast(uint8_t x, uint8_t y, uint8_t addr) {
  lcd.setCursor(x, y);
  lcd.print(&str_op[addr][0]);
}

void display_opcode(uint8_t x, uint8_t y, uint8_t addr) {
  char str_op_tmp[12];
  lcd.setCursor(x, y);
  switch (rom[addr] >> 4) {
    case ADD_A_Im:
      snprintf(str_op_tmp, 12, "ADD  A , %X ", rom[addr] & 0b00001111);
      break;
    case MOV_A_B:
      snprintf(str_op_tmp, 12, "MOV  A , B ");
      break;
    case IN_A:
      snprintf(str_op_tmp, 12, "IN   A     ");
      break;
    case MOV_A_Im:
      snprintf(str_op_tmp, 12, "MOV  A , %X ", rom[addr] & 0b00001111);
      break;
    case MOV_B_A:
      snprintf(str_op_tmp, 12, "MOV  B , A ");
      break;
    case ADD_B_Im:
      snprintf(str_op_tmp, 12, "ADD  B , %X ", rom[addr] & 0b00001111);
      break;
    case IN_B:
      snprintf(str_op_tmp, 12, "IN   B     ");
      break;
    case MOV_B_Im:
      snprintf(str_op_tmp, 12, "MOV  B , %X ", rom[addr] & 0b00001111);
      break;
    case OUT_Im1:
      snprintf(str_op_tmp, 12, "OUT  %X     ", rom[addr] & 0b00001111);
      break;
    case OUT_B:
      snprintf(str_op_tmp, 12, "OUT  B     ");
      break;
    case OUT_Im2:
      snprintf(str_op_tmp, 12, "OUT  %X     ", rom[addr] & 0b00001111);
      break;
    case OUT_Im3:
      snprintf(str_op_tmp, 12, "OUT  %X     ", rom[addr] & 0b00001111);
      break;
    case JMP_B1:
      snprintf(str_op_tmp, 12, "JMP  B     ");
      break;
    case JMP_B2:
      snprintf(str_op_tmp, 12, "JMP  B     ");
      break;
    case JNC_Im:
      snprintf(str_op_tmp, 12, "JNC  %X     ", rom[addr] & 0b00001111);
      break;
    case JMP_Im:
      snprintf(str_op_tmp, 12, "JMP  %X     ", rom[addr] & 0b00001111);
      break;
    default:
      break;
  }
  lcd.print(str_op_tmp);
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
    switch (read_LCD_buttons(analogRead(0))) {
      case btnUP:
      case btnDOWN:
        arrow = (++arrow) % 2;
        break;
      case btnSELECT:
        if (arrow == 0) {
          display = load_pvos;
        } else {
          display = rom_input;
        }
      default:
        break;
    }
    delay(ttn);
    lcd.setCursor(0, 0);
    if (arrow == 0) {
      lcd.print("load previous <-");
      lcd.setCursor(0, 1);
      lcd.print("create new      ");
    } else {
      lcd.print("load previous   ");
      lcd.setCursor(0, 1);
      lcd.print("create new    <-");
    }
  }
}
// display:0 mode:0
// ----------------
// 0123456789ABCDEF
// fast mode <-
// slow mode <-
void display_5() {
  while (display == mode_select) {
    switch (read_LCD_buttons(analogRead(0))) {
      case btnUP:
        if (mode == 0) {
          mode = 2;
        } else {
          mode--;
        }
        break;
      case btnDOWN:
        if (mode == 2) {
          mode = 0;
        } else {
          mode++;
        }
        break;
      case btnSELECT:
        display = load_or_new;
      default:
        break;
    }
    delay(ttn);
    lcd.setCursor(0, 0);
    if (mode == 0) {
      lcd.print("100Hz <- fast   ");
      lcd.setCursor(0, 1);
      lcd.print(" 1 Hz           ");
    } else if (mode == 1) {
      lcd.print("100Hz    fast   ");
      lcd.setCursor(0, 1);
      lcd.print(" 1 Hz <-        ");
    } else {
      lcd.print("100Hz    fast <-");
      lcd.setCursor(0, 1);
      lcd.print(" 1 Hz           ");
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
// SAVE:<#0>
//
// ----------------
// 0123456789ABCDEF
// RUN the program
// push select
void display_1() {
  uint8_t address = 0x00;
  uint8_t bit = 0;
  uint8_t num = num_save;
  uint8_t cng_flag = 1;
  lcd.cursor();
  while (display == rom_input) {
    switch (read_LCD_buttons(analogRead(0))) {
      case btnUP:
        if (address == 0) {
          address = 17;
        } else {
          address = (--address) % 18;  //0～15は編集するROMのアドレスに対応、16はromの保存先選択画面、17は実行選択画面
        }
        cng_flag = 1;
        break;
      case btnDOWN:
        address = (++address) % 18;
        cng_flag = 1;
        break;
      case btnLEFT:
        if (address == 16) {
          num = (--num) % 8;
        } else if (address < 16) {
          bit = (++bit) % 8;
        }
        break;
      case btnRIGHT:
        if (address == 16) {
          num = (++num) % 8;
        } else if (address < 16) {
          bit = (--bit) % 8;
        }
        break;
      case btnSELECT:
        if (address == 17) {
          display = run_pgm;
          rom_save(num);
          lcd.noCursor();
        } else if (address < 16) {
          cng_flag = 1;
          rom[address] ^= (1 << bit);
        }
        break;
      default:
        break;
    }
    lcd.setCursor(0, 0);
    if (address == 16) {
      lcd.print("SAVE:<# >       ");
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(7, 0);
      lcd.print(num, DEC);
      lcd.setCursor(7, 0);
    } else if (address == 17) {
      lcd.print("RUN the program ");
      lcd.setCursor(0, 1);
      lcd.print("push select     ");
    } else {
      if (cng_flag) {
        lcd.print("0x0             ");
        lcd.setCursor(3, 0);
        lcd.print(address, HEX);
        lcd.setCursor(5, 0);
        display_opcode(5, 0, address);
        for (int i = 0; i < 8; i++) {
          lcd.setCursor(0xE - 2 * i, 1);
          if ((rom[address] & (1 << i)) == 0) {
            lcd.print("0 ");
          } else {
            lcd.print("1 ");
          }
        }
        cng_flag = 0;
      }
      lcd.setCursor(0xE - 2 * bit, 1);
    }
    delay(ttn + 50);
  }
}

// display:2
// ----------------
// 0123456789ABCDEF
// m 00 ***********
// A*B*S*c*s:******
void display_2() {
  lcd.setCursor(0, 0);
  char str_mode_addr[2];
  snprintf(str_mode_addr, 5, "PC:%X ", reg_pc);
  lcd.print(str_mode_addr);
  display_opcode_fast(5, 0, reg_pc);
  char str[16];
  snprintf(str, 16, "%XB%XO%Xc%Xs:%06lu", reg_a, reg_b, reg_out, c_flag, score);
  lcd.setCursor(1, 1);
  lcd.print(str);
  if (mode == 1) {
    delay(1000);
  }
}

// display:3 arrow_x:2 arrow_y:1
// ----------------
// 0123456789ABCDEF
// #0  #1  #2  #3
// #4  #5  #6<-#7
void display_3() {
  uint8_t arrow = 0;
  while (display == load_pvos) {
    switch (read_LCD_buttons(analogRead(0))) {
      case btnUP:
      case btnDOWN:
        arrow = (arrow + 4) % 8;
        break;
      case btnRIGHT:
        arrow = (++arrow) % 8;
        break;
      case btnLEFT:
        arrow = (--arrow) % 8;
        break;
      case btnSELECT:
        display = rom_input;
        rom_load(arrow);
        break;
      default:
        break;
    }
    lcd.setCursor(0, 0);
    switch (arrow) {
      case 0:
        lcd.print("#0<-#1  #2  #3  ");
        lcd.setCursor(0, 1);
        lcd.print("#4  #5  #6  #7  ");
        break;
      case 1:
        lcd.print("#0  #1<-#2  #3  ");
        lcd.setCursor(0, 1);
        lcd.print("#4  #5  #6  #7  ");
        break;
      case 2:
        lcd.print("#0  #1  #2<-#3  ");
        lcd.setCursor(0, 1);
        lcd.print("#4  #5  #6  #7  ");
        break;
      case 3:
        lcd.print("#0  #1  #2  #3<-");
        lcd.setCursor(0, 1);
        lcd.print("#4  #5  #6  #7  ");
        break;
      case 4:
        lcd.print("#0  #1  #2  #3  ");
        lcd.setCursor(0, 1);
        lcd.print("#4<-#5  #6  #7  ");
        break;
      case 5:
        lcd.print("#0  #1  #2  #3  ");
        lcd.setCursor(0, 1);
        lcd.print("#4  #5<-#6  #7  ");
        break;
      case 6:
        lcd.print("#0  #1  #2  #3  ");
        lcd.setCursor(0, 1);
        lcd.print("#4  #5  #6<-#7  ");
        break;
      case 7:
        lcd.print("#0  #1  #2  #3  ");
        lcd.setCursor(0, 1);
        lcd.print("#4  #5  #6  #7<-");
        break;
      default:
        break;
    }
    num_save = arrow;
    delay(ttn);
  }
}

void display_4(uint32_t pc) {
  char str[17];
  char str_mode_addr[6];
  lcd.clear();
  snprintf(str_mode_addr, 6, "* %X ", pc);
  lcd.print(str_mode_addr);
  display_opcode(5, 0, pc);
  while (display == end_emu) {
    switch (read_LCD_buttons(analogRead(0))) {
      case btnUP:
        if (show != 0) {
          show--;
        }
        break;
      case btnDOWN:
        if (show != 1) {
          show++;
        }
        break;
      default:
        break;
    }

    lcd.setCursor(0, 1);
    switch (show) {
      case 0:
        snprintf(str, 16, "A:%X B:%X O:%X c:%X ", reg_a, reg_b, reg_out, c_flag);
        break;
      case 1:
        snprintf(str, 16, " score:%06lu  ", score);
        break;
      default:
        break;
    }
    lcd.print(str);
    delay(ttn);
  }
}

void setup() {
  // pin割り当てと初期化
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  pinMode(13, INPUT_PULLUP);

  //ROM初期化
  rom_init();
  //LCD初期化
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("TD4 Ver1.01");
  delay(1000);

  while (display != run_pgm) {
    switch (display) {
      case load_or_new:
        display_0();
        break;
      case rom_input:
        display_1();
        break;
      case load_pvos:
        display_3();
        break;
      case mode_select:
        display_5();
        break;
      default:
        break;
    }
  }
  lcd.clear();
  if (mode == 2) {
    lcd.setCursor(0, 0);
    lcd.print("Running...");
  } else {
    lcd.setCursor(0, 1);
    lcd.print("A");
  }
  init_display_opcode();
}

void loop() {
  uint32_t p_pc;
  int8_t opcode = rom[reg_pc] >> 4;
  if (mode != 2) {
    display_2();
  }
  p_pc = reg_pc;
  switch (opcode) {
    case ADD_A_Im:
      add_a_im();
      break;
    case MOV_A_B:
      mov_a_b();
      break;
    case IN_A:
      in_a();
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
    case IN_B:
      in_b();
      break;
    case MOV_B_Im:
      mov_b_im();
      break;
    case OUT_Im1:
      out_im1();
      break;
    case OUT_B:
      out_b();
      break;
    case OUT_Im2:
      out_im2();
      break;
    case OUT_Im3:
      out_im3();
      break;
    case JMP_B1:
      jmp_b1();
      break;
    case JMP_B2:
      jmp_b2();
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
  // score測定
  score++;
  // 終了時の判定
  if (reg_out > 0b0111) {
    display = end_emu;
    EEPROM.write(num_save * 0x20 + 0x10, score & 0b1111000000000000 >> 12);
    EEPROM.write(num_save * 0x20 + 0x11, score & 0b0000111100000000 >> 8);
    EEPROM.write(num_save * 0x20 + 0x12, score & 0b0000000011110000 >> 4);
    EEPROM.write(num_save * 0x20 + 0x13, score & 0b0000000000001111 >> 0);
    display_4(p_pc);
  }
}
