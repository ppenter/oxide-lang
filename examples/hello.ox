// hello.ox — Oxide "Hello World" example
// Demonstrates: string literals, print, let declarations, fn calls

fn greet(name: string) -> void {
    print(name);
}

fn add(a: int, b: int) -> int {
    return a + b;
}

let msg: string = "Hello, Oxide!";
print(msg);
greet("World from Oxide!");

let result: int = add(3, 4);
print(result);
