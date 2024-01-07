#include <iostream>
#include <vector>

using namespace std;

int main()
{

    for (int i = 0; i < 64; i++) {
        vector<int> legal;
        int file = i % 8;
        int rank = i / 8;

        if (rank >= 2) {
            if (file >= 1) {
                legal.push_back(i - 17);
            }
            if (file <= 6) {
                legal.push_back(i - 15);
            }
        }
        if (rank >= 1) {
            if (file >= 2) {
                legal.push_back(i - 10);
            }
            if (file <= 5) {
                legal.push_back(i - 6);
            }
        }
        if (rank <= 6) {
            if (file >= 2) {
                legal.push_back(i + 6);
            }
            if (file <= 5) {
                legal.push_back(i + 10);
            }
        }
        if (rank <= 5) {
            if (file >= 1) {
                legal.push_back(i + 15);
            }
            if (file <= 6) {
                legal.push_back(i + 17);
            }
        }

        cout << "{" << (legal.size() + 1);

        for (int j : legal) {
            cout << ", " << j;
        }

        for (int j = 0; j < 8 - legal.size(); j++) {
            cout << ", 0";
        }

        cout << "},\n";
    }
}