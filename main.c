#include "c_json.h"

int main() {
    struct simple_json json = parse_simple_json("./example.json");
    print_simple_json(json);
    return 0;
}
