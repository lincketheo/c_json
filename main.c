#include "c_json.h"

int main() {
    struct sj_number num;
    struct simple_json json = parse_simple_json("./example.json");
    print_simple_json(json);
    return 0;
}
