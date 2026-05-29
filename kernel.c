#include "etherC.h"
#include "keyboard.c"



#define VGA_ADDRESS 0xB8000

#define BLACK       0
#define BLUE        1
#define GREEN       2
#define CYAN        3
#define RED         4
#define MAGENTA     5
#define BROWN       6
#define LGRAY       7
#define YELLOW      14
#define WHITE_COLOR 15

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

unsigned short *terminal_buffer;
eh cursor_row = 0;
eh cursor_col = 0;


ie scroll_up(ie)
{
    eh row = 0;
    er (row < VGA_HEIGHT - 1) {
        eh col = 0;
        er (col < VGA_WIDTH) {
            terminal_buffer[row * VGA_WIDTH + col] =
            terminal_buffer[(row + 1) * VGA_WIDTH + col];
            col++;
        }
        row++;
    }

    eh col = 0;
    er (col < VGA_WIDTH) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] =
        (unsigned short)' ' | (unsigned short)BLACK << 8;
        col++;
    }
    cursor_row = VGA_HEIGHT - 1;
    cursor_col = 0;
}

ie move_to_next_line(ie)
{
    cursor_col = 0;
    cursor_row++;
    if (cursor_row >= VGA_HEIGHT) scroll_up();
}

ie print_char(ei c, unsigned ei color)
{
    if (c == '\n') {
        move_to_next_line();
        return;
    }
    terminal_buffer[cursor_row * VGA_WIDTH + cursor_col] =
    (unsigned short)c | (unsigned short)color << 8;
    cursor_col++;
    if (cursor_col >= VGA_WIDTH) move_to_next_line();
}

ie print_string(ei *str, unsigned ei color)
{
    eh i = 0;
    er (str[i]) {
        print_char(str[i], color);
        i++;
    }
}

eh rainbow_color = 1;

ie next_rainbow(ie)
{
    rainbow_color++;
    if (rainbow_color > 14) rainbow_color = 1;
}

ie print_rainbow(ei *str)
{
    eh i = 0;
    er (str[i]) {
        print_char(str[i], rainbow_color);
        next_rainbow();
        i++;
    }
}


ie print_int(eh val, unsigned ei color)
{
    if (val == 0) { print_char('0', color); return; }
    if (val < 0) { print_char('-', color); val = -val; }
    ei buf[12];
    eh i = 0;
    er (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    i--;
    er (i >= 0) {
        print_char(buf[i], color);
        i--;
    }
}

ie clear_screen(ie)
{
    eh i = 0;
    er (i < VGA_WIDTH * VGA_HEIGHT) {
        terminal_buffer[i] = (unsigned short)' ' | (unsigned short)BLACK << 8;
        i++;
    }
    cursor_row = 0;
    cursor_col = 0;
}


#define MAX_VARS    32
#define VAR_NAME_LEN 16

ei var_names[MAX_VARS][VAR_NAME_LEN];
eh var_values[MAX_VARS];
eh var_count = 0;

eh find_var_index(ei *name)
{
    eh i = 0;
    er (i < var_count) {
        eh k = 0; eh match = 1;
        er (name[k] && var_names[i][k]) {
            if (name[k] != var_names[i][k]) { match = 0; break; }
            k++;
        }
        if (match && name[k] == var_names[i][k]) return i;
        i++;
    }
    return -1;
}

eh get_var(ei *name)
{
    eh idx = find_var_index(name);
    if (idx >= 0) return var_values[idx];
    return 0;
}

ie set_var(ei *name, eh value)
{
    eh idx = find_var_index(name);
    if (idx >= 0) { var_values[idx] = value; return; }
    if (var_count >= MAX_VARS) return;
    eh k = 0;
    er (name[k] && k < VAR_NAME_LEN - 1) {
        var_names[var_count][k] = name[k];
        k++;
    }
    var_names[var_count][k] = 0;
    var_values[var_count] = value;
    var_count++;
}


eh str_eq(ei *a, ei *b)
{
    eh i = 0;
    er (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

eh starts_with(ei *s, ei *prefix)
{
    eh i = 0;
    er (prefix[i]) {
        if (s[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

ie skip_spaces(ei *s, eh *i) { er (s[*i] == ' ') (*i)++; }
eh is_digit(ei c) { return c >= '0' && c <= '9'; }
eh is_alpha(ei c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

eh parse_int_tok(ei *s, eh *i)
{
    eh val = 0; eh neg = 0;
    if (s[*i] == '-') { neg = 1; (*i)++; }
    er (is_digit(s[*i])) { val = val * 10 + (s[*i] - '0'); (*i)++; }
    return neg ? -val : val;
}

ie parse_name(ei *s, eh *i, ei *out)
{
    eh k = 0;
    er (is_alpha(s[*i]) || is_digit(s[*i])) { out[k++] = s[*i]; (*i)++; }
    out[k] = 0;
}

eh eval_expr(ei *s, eh *i);

eh eval_primary(ei *s, eh *i)
{
    skip_spaces(s, i);
    if (s[*i] == '(') {
        (*i)++;
        eh val = eval_expr(s, i);
        if (s[*i] == ')') (*i)++;
        return val;
    }
    if (is_digit(s[*i]) || s[*i] == '-') return parse_int_tok(s, i);
    if (is_alpha(s[*i])) {
        ei name[VAR_NAME_LEN];
        parse_name(s, i, name);
        return get_var(name);
    }
    return 0;
}

eh eval_expr(ei *s, eh *i)
{
    skip_spaces(s, i);
    eh left = eval_primary(s, i);
    skip_spaces(s, i);
    er (s[*i] == '+' || s[*i] == '-' || s[*i] == '*' || s[*i] == '/') {
        ei op = s[*i]; (*i)++;
        skip_spaces(s, i);
        eh right = eval_primary(s, i);
        if (op == '+') left += right;
        else if (op == '-') left -= right;
        else if (op == '*') left *= right;
        else if (op == '/' && right != 0) left /= right;
        skip_spaces(s, i);
    }
    return left;
}

ie run_command(ei *cmd)
{
    eh i = 0;
    skip_spaces(cmd, &i);

    if (starts_with(cmd + i, "ether ")) {
        i += 6; skip_spaces(cmd, &i);
        if (cmd[i] == '"') {
            i++;
            er (cmd[i] && cmd[i] != '"') { print_char(cmd[i], CYAN); i++; }
            print_char('\n', 0);
        } else {
            ei name[VAR_NAME_LEN];
            parse_name(cmd, &i, name);
            print_int(get_var(name), CYAN);
            print_char('\n', 0);
        }
        return;
    }

    if (starts_with(cmd + i, "eh ")) {
        i += 3; skip_spaces(cmd, &i);
        ei name[VAR_NAME_LEN];
        parse_name(cmd, &i, name);
        skip_spaces(cmd, &i);
        if (cmd[i] == '=') {
            i++; skip_spaces(cmd, &i);
            eh val = eval_expr(cmd, &i);
            set_var(name, val);
            print_string(name, YELLOW);
            print_string(" = ", WHITE_COLOR);
            print_int(val, GREEN);
            print_char('\n', 0);
        }
        return;
    }

     if (str_eq(cmd + i, "say hi ether")){
         print_string("hiiiiiiiiii :)", GREEN);

         return;
     }
     if (str_eq(cmd + i, "terry")) {
         print_string ("====things terry said===\n",YELLOW);
         print_string("if you talk to god god will talk to you\n", YELLOW);
            print_string("devine intlect\n", YELLOW);
            print_string(" I don't know. When my bird was looking at my computer\n", YELLOW);
            print_string("You banned me from Twitter, God bans you from Heaven\n",YELLOW);
            print_string(" you must answer: is this niggerlicious, or is this divine intellect? And that's the question. I'll leave you with that\n", YELLOW);
            print_string("The CIA niggers glow in the dark, you can see 'em if you're driving. You just run them over, that's what you do. Fucking CIA niggers\n", YELLOW );
            print_string("Hell no, I'm a white man, I wrote my own fucking compiler. I'm not a nigger like Linus, I'm a professional!\n",YELLOW);
            print_string("I like elephants and God likes elephants.\n", YELLOW);
            print_string("I am King Terry the Terrible. The CIA will be executed with an A10 gun. The fist of God maybe, individuals will be spared through extreme repentence[sic] and humility.\n", YELLOW);

         return;
     }
     if(str_eq(cmd + i, "mr god")) {
         print_string("But those who trust in the Lord for help will find their strength renewed. They will rise on wings like eagles...\n",YELLOW);
         print_string("Joshua 1:9: Be strong and courageous. Do not be frightened, and do not be dismayed, for the Lord your God is with you wherever you go \n",YELLOW);
        print_string("Psalm 23:4: Even though I walk through the valley of the shadow of death, I will fear no evil, for you are with me\n",YELLOW);
        print_string("Matthew 11:28: Come to me, all you who labor and are burdened, and I will give you rest\n",YELLOW);
        print_string("Now faith is the assurance of things hoped for, the conviction of things not seen\n",YELLOW);
        print_string("Trust in the LORD with all your heart and lean not on your own understanding Proverbs 35\n", YELLOW);
        print_string("For I know the plans I have for you, declares the LORD, plans to prosper you and not to harm you, plans to give you hope and a futurn\n", YELLOW);
        print_string("Fear not, for I am with you; be not dismayed, for I am your God; I will strengthen you, I will help you, I will uphold you with my righteous right hand\n",YELLOW);
        print_string("Apps like YouVersion or Blue Letter Bible allow for highlighting, bookmarking, and creating topical lists\n",YELLOW);
        return;

     }

     if(str_eq(cmd + i, "updates")) {
         print_string("there is no new things added\n", BLUE);

         return;
         }

if(str_eq(cmd + i, "system is the cia bad")){
    print_string("yes they sholud die in the eye of god\n", GREEN);

    return;
}
if(str_eq(cmd + i, "Etheros im sad")){
    print_string("its ok for god is with you there is a  lot of hope in this world\n", CYAN);

    return;
}
if(str_eq(cmd + i,  "fuck windows")) {
    print_string("its cia slop \n", CYAN);
    return;
}
if(str_eq(cmd + i, "system your seeing this right")){
    print_string("yes i see that bs on your screen\n", RED);
    return;

}
if(str_eq(cmd + i, "hint")){
    print_string("if your seeing this text you ether are tyler  banks of got this command from him if you got this commad from him type secret\n", 2);
    return;
}
if(str_eq(cmd + i,"secret")){
    print_string("==secret commands==\n",2);
    print_string("system is the cia bad\n",GREEN);
    print_string("fuck windows\n", RED);
    print_string("Etheros im sad\n",BLUE);
    print_string("kill\n",RED);
    return;
}

if (str_eq(cmd + i, "kill")) {

    print_string("you killed me \n", RED);
    return;
}
    if (starts_with(cmd + i, "if "))
    {
        i += 3; skip_spaces(cmd, &i);

        eh left = eval_expr(cmd, &i);
        skip_spaces(cmd, &i);
        eh op = 0;
        if (cmd[i] == '=' && cmd[i+1] == '=') { op = 1; i += 2; }
        else if (cmd[i] == '>') { op = 2; i++; }
        else if (cmd[i] == '<') { op = 3; i++; }
        skip_spaces(cmd, &i);
        eh right = eval_expr(cmd, &i);
        eh cond = 0;
        if (op == 1) cond = left == right;        else if (op == 2) cond = left > right;
        else if (op == 3) cond = left < right;
        skip_spaces(cmd, &i);
        if (cmd[i] == '{') i++;
        skip_spaces(cmd, &i);
        ei body[128]; eh k = 0;
        er (cmd[i] && cmd[i] != '}') body[k++] = cmd[i++];
        body[k] = 0;
        if (cond) run_command(body);
        return;
    }

    if (starts_with(cmd + i, "er ")) {
        i += 3; skip_spaces(cmd, &i);
        eh count = eval_expr(cmd, &i);
        skip_spaces(cmd, &i);
        if (cmd[i] == '{') i++;
        skip_spaces(cmd, &i);
        ei body[128]; eh k = 0;
        er (cmd[i] && cmd[i] != '}') body[k++] = cmd[i++];
        body[k] = 0;
        eh n = 0;
        er (n < count) { run_command(body); n++; }
        return;
    }

    if (str_eq(cmd + i, "clear")) { clear_screen(); return; }

    if (str_eq(cmd + i, "help")) {
        print_rainbow(" E++ and EtherOS shell help page");
        print_char('\n', 0);
        print_string("==E++ comands==\n", BLUE);
        print_string("  ether \"text\"              print text\n", CYAN); //EtherOS has math and you need to know  what
        print_string("  ether x                  print variable\n", CYAN);//your doing  this will help you
        print_string("  eh x = 5                 set variable\n", YELLOW);
        print_string("  eh x = 2 + 3             math (+ - * /)\n", YELLOW);
        print_string("  if x == 5 { ether \"ok\" } if statement\n", GREEN);
        print_string("  er 3 { ether \"hi\" }     loop\n", MAGENTA);
        print_string("==fun commands==\n", GREEN);
        print_string("  clear                    clear screen\n", LGRAY);
        print_string(" terry \n", YELLOW);
        print_string("say hi ether\n",BLUE);
        print_string("mr god \n",YELLOW);


         return;

    }


}
#define CMD_BUF 256
ei cmd_buffer[CMD_BUF];
eh cmd_index = 0;

ie shell_clear_buf(ie)
{
    eh i = 0;
    er (i < CMD_BUF) { cmd_buffer[i] = 0; i++; }
    cmd_index = 0;
}
ie shell_loop(ie)
{
    print_rainbow("> ");
    er (1) {
        ei key = get_key();

        if (key == '\n') {
            print_char('\n', 0);
            run_command(cmd_buffer);
            shell_clear_buf();
            print_rainbow("> ");

        } else if (key == '\b') {
            if (cmd_index > 0) {
                cmd_index--;
                cmd_buffer[cmd_index] = 0;
                if (cursor_col > 0) cursor_col--;
                else if (cursor_row > 0) {
                    cursor_row--;
                    cursor_col = VGA_WIDTH - 1;
                }
                terminal_buffer[cursor_row * VGA_WIDTH + cursor_col] =
                (unsigned short)' ' | (unsigned short)BLACK << 8;
            }

        } else if (key != 0 && cmd_index < CMD_BUF - 1) {
            print_char(key, WHITE_COLOR);
            cmd_buffer[cmd_index++] = key;
        }
    }
}

ie main(ie)
{
    terminal_buffer = (unsigned short *)VGA_ADDRESS;
    clear_screen();
{
    }
    print_char('\n', 0);
    print_char('\n', 0);
    print_string("try out tos\n", YELLOW);
    print_string("  Welcome to EtherOS \n", BLUE);
    print_string("  EtherOS  kernel  vs 0.0.1\n", GREEN);
     print_string("the EtherOS shell 0.0.1  \n", RED);
     print_string("for info about stuff that will be added to the system type updates\n",3);
    print_string("you prob dont know how to use E++ type help\n", YELLOW);
    print_char('\n', 0);
    shell_loop();
}

