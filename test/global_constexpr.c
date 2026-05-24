// Check all kinds of operators in the global scope, i.e check constant folding
static const int a = ((3 * 2 + 7) / 4) << 1;  // = 6
static const unsigned b = (unsigned int)(a + (-1));  // = 5
static const bool c = (a != b);  // = true
static const int k = (-9) >> 2;  // = -3
static const unsigned n = 9 >> 2;  // = 2
static const short result = (short)((int)b + k - (int)n);  // = 0
static const short* p = &result;

int main() {
    if (c)
        return (int)(*p);
    else
        return a;
}
