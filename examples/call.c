int a(int y) {
    return y + 1;
}

int b(int x, int y) {
    if (x > 0)
        return b(x - 1, y) + a(x);
    else
        return a(x);
}

int main(int argc, char* argv) {
    return b(10, 2) + a(0);
}
