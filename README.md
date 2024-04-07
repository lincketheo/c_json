# Version 0.0.1

**State:** Green path works. (Try main.c)

**Need to do Features:**

- Test various edge cases
- Lots of error handling (particularly when fgetc returns NULL, I often treat those as chars)
- Add arbitrary stream so you can parse a const char instead of only files
- JSON Path - For now, you'll have to manually get stuff from data structures
- e is currently broken in parse\_num
- Add detailed context when error
- Handle \uXXXX in string
- Make sj\_object a hashmap instead of list
- Add real tests

# Usage:

The main object you should be interested in is `simple_json`

```c
#include "c_json.h"

int main() {
    struct simple_json json = parse_simple_json("./example.json");
    print_simple_json(json);
    free_simple_json(json);
    return 0;
}
```
