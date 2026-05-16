int main() {
    int a = 8;

    // This could be `addl $a, $a`, except `a` is reused later, so `a` must be saved
    // This tests `check_overwrite` and the in-out operand generation and transfer
    int b = a + a;
    int c = a + b;
    return c - 24;
}
