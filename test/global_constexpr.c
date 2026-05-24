static const int a = ((3 * 2 + 7) / 4) << 1;  // = 6
static const unsigned b = (unsigned int)(a + (-1));  // = 5
static const bool c = (a != b);  // = true
static const int k = (-9) >> 2;  // = -3
static const unsigned n = 9 >> 2;  // = 2
static const int result = (int)b + k - (int)n;  // = 0
static const int* p = &result;

int main() {
    if (c)
        return *p;
    else
        return a;
}
