// This checks deep function calls, recursivity, CFI

int a(int y) {
    return y + 1;
}

int b(int x, int y) {
    if (x > 0)
        return b(x - 1, y) + a(x);
    else
        return a(x);
}

int main() {
    return b(10, 2) - a(65);
}
