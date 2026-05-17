static const int a = (3 * 2 + 7) / 4;
static const unsigned b = (unsigned int)a - 3u;
static const unsigned* p = &b;

int main() {
    return *p;
}
