//目前只支持测评单文件程序，所以需要将代码都写到这个文件中。

#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <map>
#include <cctype>
using namespace std;

// ==================== 全局变量 ====================
map<string, int> categoryCount;
int errorCount = 0;
int line = 1;
int totalLines = 1;
char lastCh = 0;

// ==================== 关键字集合 ====================
set<string> keywords = {
    "auto", "break", "case", "char", "const", "continue", "default",
    "do", "double", "else", "enum", "extern", "float", "for", "goto",
    "if", "int", "long", "register", "return", "short", "signed",
    "sizeof", "static", "struct", "switch", "typedef", "union",
    "unsigned", "void", "volatile", "while"
};

// ==================== 分隔符集合 ====================
set<char> delimiters = { ';', ',', ':', '?', '(', ')', '[', ']', '{', '}' };

// ==================== 输出记号并统计 ====================
void outputToken(int tokLine, const string& category, const string& attr) {
    cout << tokLine << " <" << category << "," << attr << ">" << endl;
    categoryCount[category]++;
}

// ==================== 判断是否为十六进制数字 ====================
bool isHexDigit(char c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// ==================== 判断是否为八进制数字 ====================
bool isOctalDigit(char c) {
    return c >= '0' && c <= '7';
}

// ==================== 处理转义序列（返回是否合法） ====================
// 读取 \ 后面的字符，判断是否为合法转义序列
// 如果是八进制或十六进制转义，继续读取后续数字
bool handleEscape(ifstream& fin, string& token, char& ch, char& lastCh) {
    // 此时 ch == '\\'
    token.push_back(ch);
    if (!fin.get(ch)) return false;  // EOF
    lastCh = ch;

    // 简单转义字符
    if (ch == '\'' || ch == '"' || ch == '?' || ch == '\\' ||
        ch == 'a' || ch == 'b' || ch == 'f' || ch == 'n' ||
        ch == 'r' || ch == 't' || ch == 'v') {
        token.push_back(ch);
        return true;
    }

    // 八进制转义：\ 后跟 1~3 位八进制数字
        // 八进制转义：\ 后跟 1~3 位八进制数字
    if (isOctalDigit(ch)) {
        token.push_back(ch);
        int count = 1;
        while (count < 3) {
            if (!fin.get(ch)) break;        // EOF，直接结束
            lastCh = ch;
            if (!isOctalDigit(ch)) {
                fin.unget();                 // 读到非八进制字符，放回
                break;
            }
            token.push_back(ch);             // 是八进制，收进来
            count++;
        }
        return true;
    }

    // 十六进制转义：\x 后跟 1 位以上十六进制数字
    if (ch == 'x' || ch == 'X') {
        token.push_back(ch);
        if (!fin.get(ch) || !isHexDigit(ch)) {
            return false;  // \x 后面必须跟十六进制数字
        }
        lastCh = ch;
        token.push_back(ch);
        while (fin.get(ch) && isHexDigit(ch)) {
            lastCh = ch;
            token.push_back(ch);
        }
        if (fin.good()) fin.unget();
        return true;
    }

    // Unicode转义：\u 后跟4位十六进制，\U 后跟8位十六进制
    if (ch == 'u') {
        token.push_back(ch);
        for (int i = 0; i < 4; i++) {
            if (!fin.get(ch) || !isHexDigit(ch)) return false;
            lastCh = ch;
            token.push_back(ch);
        }
        return true;
    }
    if (ch == 'U') {
        token.push_back(ch);
        for (int i = 0; i < 8; i++) {
            if (!fin.get(ch) || !isHexDigit(ch)) return false;
            lastCh = ch;
            token.push_back(ch);
        }
        return true;
    }

    // 未知转义，当作普通字符处理（或报错，这里宽松处理）
    token.push_back(ch);
    return true;
}

// ==================== 读取字符串或字符常量的主体 ====================
// 返回 true 表示正常闭合
bool readQuoted(ifstream& fin, string& token, char quoteChar,
    char& ch, char& lastCh) {
    token.clear();
    token.push_back(quoteChar);
    bool closed = false;

    while (fin.get(ch)) {
        lastCh = ch;
        if (ch == '\\') {
            if (!handleEscape(fin, token, ch, lastCh)) {
                // 转义处理失败（EOF等）
                break;
            }
        }
        else if (ch == quoteChar) {
            token.push_back(ch);
            closed = true;
            break;
        }
        else if (ch == '\n') {
            fin.unget();
            break;
        }
        else {
            token.push_back(ch);
        }
    }
    return closed;
}

// ==================== 主词法分析函数 ====================
void lexicalAnalysis(const string& filename) {
    ifstream fin(filename.c_str());
    if (!fin.is_open()) {
        cerr << "无法打开文件: " << filename << endl;
        return;
    }

    char ch;
    while (fin.get(ch)) {
        lastCh = ch;

        // 1. 换行
        if (ch == '\n') {
            totalLines++;
            line++;
            continue;
        }

        // 2. 空白
        if (isspace(ch)) continue;

        int tokenLine = line;

        // 3. 预处理指令
        if (ch == '#') {
            while (fin.get(ch) && ch != '\n') {}
            if (ch == '\n') { totalLines++; line++; }
            continue;
        }

        // 4. 注释
        if (ch == '/') {
            if (fin.peek() == '/') {
                fin.get(ch);
                while (fin.get(ch) && ch != '\n') {}
                if (ch == '\n') { totalLines++; line++; }
                continue;
            }
            else if (fin.peek() == '*') {
                fin.get(ch);
                while (fin.get(ch)) {
                    if (ch == '\n') { totalLines++; line++; }
                    else if (ch == '*') {
                        if (fin.peek() == '/') { fin.get(ch); break; }
                    }
                }
                continue;
            }
        }

        // 5. 字符串常量（含前缀 u8/u/U/L）
        // 先检查是否以 u8" / u" / U" / L" 开头
        {
            bool hasPrefix = false;
            string prefix;
            // 我们需要 peek 多个字符来判断前缀
            // 由于 ifstream 的 peek 只能看一个字符，这里用更直接的方法：
            // 如果当前字符是 u/U/L，且后面紧跟引号（或u8），则进入字符串处理
            if (ch == 'u' || ch == 'U' || ch == 'L') {
                // 保存当前位置，尝试读取前缀
                string candidate;
                candidate.push_back(ch);
                // 尝试读后续字符
                if (ch == 'u') {
                    if (fin.peek() == '8') {
                        fin.get(ch); candidate.push_back(ch);
                        if (fin.peek() == '"') {
                            fin.get(ch); candidate.push_back(ch);
                            hasPrefix = true;
                        }
                        else {
                            // 不是字符串前缀，把读到的放回去
                            // 注意：unget 有限制，这里简单处理
                        }
                    }
                    else if (fin.peek() == '"') {
                        fin.get(ch); candidate.push_back(ch);
                        hasPrefix = true;
                    }
                }
                else if (ch == 'U' || ch == 'L') {
                    if (fin.peek() == '"') {
                        fin.get(ch); candidate.push_back(ch);
                        hasPrefix = true;
                    }
                }
                // 注意：这里如果 hasPrefix 为 true，candidate 已经包含了前缀和引号
                // 但我们需要把引号之后的内容继续读入
                if (hasPrefix) {
                    string token = candidate;  // 包含前缀 + 引号
                    bool closed = false;
                    while (fin.get(ch)) {
                        lastCh = ch;
                        if (ch == '\\') {
                            if (!handleEscape(fin, token, ch, lastCh)) break;
                        }
                        else if (ch == '"') {
                            token.push_back(ch);
                            closed = true;
                            break;
                        }
                        else if (ch == '\n') {
                            fin.unget();
                            break;
                        }
                        else {
                            token.push_back(ch);
                        }
                    }
                    if (closed) {
                        outputToken(tokenLine, "STRING", token);
                    }
                    else {
                        cout << tokenLine << " <ERROR," << token << ">" << endl;
                        errorCount++;
                    }
                    continue;
                }
                // 如果不是前缀，继续往下走（可能是标识符）
            }
        }

        // 6. 普通字符串（无前缀）
        if (ch == '"') {
            string token;
            bool closed = readQuoted(fin, token, '"', ch, lastCh);
            if (closed) {
                outputToken(tokenLine, "STRING", token);
            }
            else {
                cout << tokenLine << " <ERROR," << token << ">" << endl;
                errorCount++;
            }
            continue;
        }

        // 7. 字符常量（含前缀 u/U/L）
        if (ch == '\'') {
            string token;
            bool closed = readQuoted(fin, token, '\'', ch, lastCh);
            if (closed) {
                outputToken(tokenLine, "CHARCON", token);
            }
            else {
                cout << tokenLine << " <ERROR," << token << ">" << endl;
                errorCount++;
            }
            continue;
        }

        // 检查带前缀的字符常量 u' U' L'
        // 检查带前缀的字符常量 u' U' L'
        if (ch == 'u' || ch == 'U' || ch == 'L') {
            if (fin.peek() == '\'') {
                string prefix(1, ch);    // 先保存前缀 u/U/L
                fin.get(ch);              // 读取开始引号 '
                lastCh = ch;

                string token;
                token += prefix;          // token = "L"
                token.push_back('\'');    // token = "L'"

                bool closed = false;
                while (fin.get(ch)) {
                    lastCh = ch;
                    if (ch == '\\') {
                        if (!handleEscape(fin, token, ch, lastCh)) break;
                    }
                    else if (ch == '\'') {
                        token.push_back(ch);   // token = "L'1'"
                        closed = true;
                        break;
                    }
                    else if (ch == '\n') {
                        fin.unget();
                        break;
                    }
                    else {
                        token.push_back(ch);
                    }
                }

                if (closed) {
                    outputToken(tokenLine, "CHARCON", token);
                }
                else {
                    cout << tokenLine << " <ERROR," << token << ">" << endl;
                    errorCount++;
                }
                continue;
            }
        }

        // 8. 标识符 / 关键字
        if (isalpha(ch) || ch == '_') {
            string token;
            token.push_back(ch);
            while (fin.get(ch) && (isalnum(ch) || ch == '_')) {
                lastCh = ch;
                token.push_back(ch);
            }
            if (fin.good()) fin.unget();
            if (keywords.find(token) != keywords.end()) {
                outputToken(tokenLine, "KEYWORD", token);
            }
            else {
                outputToken(tokenLine, "IDENTIFIER", token);
            }
            continue;
        }

        // 9. 数值常量（整数、浮点、八进制、十六进制）
        if (isdigit(ch) || (ch == '.' && isdigit(fin.peek()))) {
            string token;
            token.push_back(ch);
            bool hasDot = false;
            bool hasExp = false;      // 十进制指数 e/E
            bool hasBinExp = false;   // 十六进制二进制指数 p/P
            bool isHex = false;
            bool isOctal = false;
            bool valid = true;

            // 判断进制
            if (ch == '0') {
                if (fin.peek() == 'x' || fin.peek() == 'X') {
                    fin.get(ch); token.push_back(ch);
                    isHex = true;
                }
                else {
                    isOctal = true;  // 可能是八进制，也可能就是0
                }
            }

            while (fin.get(ch)) {
                lastCh = ch;
                if (isHex) {
                    // 十六进制：允许 0-9a-fA-F，可能包含 . 和 p/P
                    if (isHexDigit(ch)) {
                        token.push_back(ch);
                    }
                    else if (ch == '.' && !hasDot && !hasBinExp) {
                        hasDot = true;
                        token.push_back(ch);
                    }
                    else if ((ch == 'p' || ch == 'P') && !hasBinExp) {
                        hasBinExp = true;
                        token.push_back(ch);
                        if (fin.peek() == '+' || fin.peek() == '-') {
                            fin.get(ch); lastCh = ch; token.push_back(ch);
                        }
                    }
                    else {
                        break;
                    }
                }
                else if (isOctal && !hasDot && !hasExp) {
                    // 八进制：允许 0-7
                    if (isOctalDigit(ch)) {
                        token.push_back(ch);
                    }
                    else if (ch == '.' && !hasDot) {
                        // 八进制后面跟小数点，实际上是十进制浮点
                        // 但按照C标准，0 开头后面跟 . 是浮点
                        hasDot = true;
                        isOctal = false;  // 转为十进制浮点处理
                        token.push_back(ch);
                    }
                    else if (ch == 'e' || ch == 'E') {
                        hasExp = true;
                        isOctal = false;  // 转为十进制浮点处理
                        token.push_back(ch);
                        if (fin.peek() == '+' || fin.peek() == '-') {
                            fin.get(ch); lastCh = ch; token.push_back(ch);
                        }
                    }
                    else {
                        break;
                    }
                }
                else {
                    // 十进制整数或浮点
                    if (isdigit(ch)) {
                        token.push_back(ch);
                    }
                    else if (ch == '.' && !hasDot && !hasExp) {
                        hasDot = true;
                        token.push_back(ch);
                    }
                    else if ((ch == 'e' || ch == 'E') && !hasExp) {
                        hasExp = true;
                        token.push_back(ch);
                        if (fin.peek() == '+' || fin.peek() == '-') {
                            fin.get(ch); lastCh = ch; token.push_back(ch);
                        }
                    }
                    else {
                        break;
                    }
                }
            }

            // 如果后面紧跟字母或下划线（且不是合法的后缀），说明是数字开头的非法标识符
            // 循环结束后：只有不是 EOF 才进行后缀检查
            if (!fin.eof()) {
                if (isalpha(ch) || ch == '_') {
                    // 后缀检查（原逻辑不变）
                    string suffix;
                    suffix.push_back(ch);
                    while (fin.get(ch) && (isalpha(ch) || ch == '_')) {
                        lastCh = ch;
                        suffix.push_back(ch);
                    }
                    if (fin.good()) fin.unget();

                    bool validSuffix = false;
                    if (!isHex && !hasDot && !hasExp) {
                        string s = suffix;
                        for (char& c : s) c = tolower(c);
                        if (s == "u" || s == "l" || s == "ul" || s == "lu" ||
                            s == "ll" || s == "ull" || s == "llu") {
                            validSuffix = true;
                            token += suffix;
                        }
                    }
                    else if (hasDot || hasExp || hasBinExp) {
                        if (suffix == "f" || suffix == "F" ||
                            suffix == "l" || suffix == "L") {
                            validSuffix = true;
                            token += suffix;
                        }
                    }

                    if (!validSuffix) {
                        token += suffix;
                        cout << tokenLine << " <ERROR," << token << ">" << endl;
                        errorCount++;
                        continue;
                    }
                }
                else {
                    if (fin.good()) fin.unget();
                }
            }

            // 检查数值合法性
            if (hasExp) {
                size_t ePos = token.find_first_of("eE");
                if (ePos != string::npos) {
                    size_t pos = ePos + 1;
                    if (pos < token.size() && (token[pos] == '+' || token[pos] == '-')) pos++;
                    if (pos >= token.size() || !isdigit(token[pos])) valid = false;
                }
            }
            if (hasBinExp) {
                size_t pPos = token.find_first_of("pP");
                if (pPos != string::npos) {
                    size_t pos = pPos + 1;
                    if (pos < token.size() && (token[pos] == '+' || token[pos] == '-')) pos++;
                    if (pos >= token.size() || !isdigit(token[pos])) valid = false;
                }
            }
            // 十六进制浮点必须要有 p/P
            if (isHex && hasDot && !hasBinExp) valid = false;
            // 八进制数字中不能出现 8 或 9（已经在读取时排除）
            // 小数点数量检查
            int dotCount = 0;
            for (char c : token) if (c == '.') dotCount++;
            if (dotCount > 1) valid = false;

            if (valid) {
                outputToken(tokenLine, "NUMBER", token);
            }
            else {
                cout << tokenLine << " <ERROR," << token << ">" << endl;
                errorCount++;
            }
            continue;
        }

        // 10. 分隔符
        if (delimiters.find(ch) != delimiters.end()) {
            string token;
            token.push_back(ch);
            outputToken(tokenLine, "DELIMITER", token);
            continue;
        }

        // 11. 运算符
        string op;
        op.push_back(ch);

        if (ch == '+') {
            if (fin.peek() == '+') { fin.get(ch); op += ch; }
            else if (fin.peek() == '=') { fin.get(ch); op += ch; }
        }
        else if (ch == '-') {
            if (fin.peek() == '-') { fin.get(ch); op += ch; }
            else if (fin.peek() == '=') { fin.get(ch); op += ch; }
            else if (fin.peek() == '>') { fin.get(ch); op += ch; }
        }
        else if (ch == '*') {
            if (fin.peek() == '=') { fin.get(ch); op += ch; }
        }
        else if (ch == '/') {
            if (fin.peek() == '=') { fin.get(ch); op += ch; }
        }
        else if (ch == '%') {
            if (fin.peek() == '=') { fin.get(ch); op += ch; }
        }
        else if (ch == '=') {
            if (fin.peek() == '=') { fin.get(ch); op += ch; }
        }
        else if (ch == '!') {
            if (fin.peek() == '=') { fin.get(ch); op += ch; }
        }
        else if (ch == '<') {
            if (fin.peek() == '<') {
                fin.get(ch); op += ch;
                if (fin.peek() == '=') { fin.get(ch); op += ch; }
            }
            else if (fin.peek() == '=') {
                fin.get(ch); op += ch;
            }
        }
        else if (ch == '>') {
            if (fin.peek() == '>') {
                fin.get(ch); op += ch;
                if (fin.peek() == '=') { fin.get(ch); op += ch; }
            }
            else if (fin.peek() == '=') {
                fin.get(ch); op += ch;
            }
        }
        else if (ch == '&') {
            if (fin.peek() == '&') { fin.get(ch); op += ch; }
            else if (fin.peek() == '=') { fin.get(ch); op += ch; }
        }
        else if (ch == '|') {
            if (fin.peek() == '|') { fin.get(ch); op += ch; }
            else if (fin.peek() == '=') { fin.get(ch); op += ch; }
        }
        else if (ch == '^') {
            if (fin.peek() == '=') { fin.get(ch); op += ch; }
        }
        else if (ch == '.') {
            // 单独的 . 是运算符
        }
        else if (ch == '~') {

        }
        else if (ch == '@') {
            cout << tokenLine << " <ERROR," << ch << ">" << endl;
            errorCount++;
            continue;
        }
        else {
            cout << tokenLine << " <ERROR," << ch << ">" << endl;
            errorCount++;
            continue;
        }

        outputToken(tokenLine, "OPERATOR", op);
    }

    fin.close();

    if (lastCh == '\n') totalLines--;
    if (totalLines < 1) totalLines = 1;

    cout << totalLines << endl;
    cout << categoryCount["KEYWORD"] << " "
        << categoryCount["IDENTIFIER"] << " "
        << categoryCount["OPERATOR"] << " "
        << categoryCount["DELIMITER"] << " "
        << categoryCount["CHARCON"] << " "
        << categoryCount["STRING"] << " "
        << categoryCount["NUMBER"] << endl;
    cout << errorCount << endl;
}


int main(int argc, char* argv[])
{

    string file_name = argv[1]; //该字符串变量为识别的C语言程序文件名

    /*程序每次运行只需要考虑识别一个C语言程序;
      需要使用读取文件的操作,可以在自己的本地环境里创建样例文件进行测试；
      在自己的IDE里测试时可以将file_name赋其他的值，
      但注意，最终测评时由平台提供的main参数来传入文件名*/
    lexicalAnalysis(file_name);


    return 0;
}