// control_flow.ox — Oxide control flow example
// Demonstrates: if/else, while, bool literals, comparisons

fn isEven(n: int) -> bool {
    return n % 2 == 0;
}

fn countDown(start: int) -> void {
    let i: int = start;
    while (i >= 0) {
        print(i);
        i = i - 1;
    }
}

fn clamp(val: int, low: int, high: int) -> int {
    if (val < low) {
        return low;
    } else {
        if (val > high) {
            return high;
        } else {
            return val;
        }
    }
}

let x: int = 7;
let even: bool = isEven(x);
print(even);      // false

countDown(3);     // prints: 3 2 1 0

let clamped: int = clamp(150, 0, 100);
print(clamped);   // 100
