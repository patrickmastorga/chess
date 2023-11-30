#include <iostream>

using namespace std;

int main()
{
    int peiceValues[15] = {0, 100, 300, 300, 500, 900, 0, 0, 0, -100, -300, -300, -500, -900, 0};

    for (int v : peiceValues) {
        cout << "    {\n";

        for (int i = 0; i < 8; ++i) {
            cout << "        ";
            for (int j = 0; j < 8; ++j) {
                cout << v << ", ";
            }
            cout << endl;
        }

        cout << "    },\n";
    }
    return 0;
}