// structs.ox — Phase 2 example: structs, for-in loops, break/continue
//
// Demonstrates:
//   - struct declarations
//   - struct literal creation
//   - field access via .
//   - for i in start..end range loops
//   - break and continue inside loops

// ── Struct declarations ───────────────────────────────────────────────────────────────────────────────

struct Point {
  x: int;
  y: int;
}

struct Rectangle {
  width: int;
  height: int;
}

// ── Functions ────────────────────────────────────────────────────────────────────────────────

// Compute Manhattan distance between two points
fn manhattan(p: Point) -> int {
  // Field access via .
  let dx: int = p.x;
  let dy: int = p.y;
  return dx + dy;
}

// Compute area of a rectangle
fn area(r: Rectangle) -> int {
  return r.width * r.height;
}

// Sum of integers in range [start, end) using for-in
fn sumRange(start: int, end: int) -> int {
  let total: int = 0;
  for i in start..end {
    total = total + i;
  }
  return total;
}

// Find first multiple of n in [0..limit) — demonstrates break
fn firstMultiple(n: int, limit: int) -> int {
  let result: int = -1;
  for i in 1..limit {
    if (i % n == 0) {
      result = i;
      break;
    }
  }
  return result;
}

// Print only odd numbers in range — demonstrates continue
fn printOdds(start: int, end: int) -> void {
  for i in start..end {
    if (i % 2 == 0) {
      continue;
    }
    print(i);
  }
}

// ── Top-level program ───────────────────────────────────────────────────────────────────────────────

// Test struct creation and field access
let p = Point { x: 3, y: 4 };
print(p.x);           // 3
print(p.y);           // 4

let dist: int = manhattan(p);
print(dist);          // 7

let r = Rectangle { width: 5, height: 8 };
let a: int = area(r);
print(a);             // 40

// Test for-in loop: sum 0..5 = 0+1+2+3+4 = 10
let s: int = sumRange(0, 5);
print(s);             // 10

// Test break: first multiple of 3 in 1..20 = 3
let fm: int = firstMultiple(3, 20);
print(fm);            // 3

// Test continue: odd numbers in 1..6 → 1, 3, 5
printOdds(1, 6);
