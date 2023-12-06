#include <iostream>
#include <vector>

using namespace std;

int main()
{

    for (int i = 0; i < 64; i++) {
        vector<int> legal;
        
        int file = i % 8;
        int rank = i / 8;
        int ifile = 7 - file;
        int irank = 7 - rank;
        int bl = std::min(rank, file);
        int br = std::min(rank, ifile);
        int fl = std::min(irank, file);
        int fr = std::min(irank, ifile);

        if (file) {
            legal.push_back(i - 1);
        }
        if (ifile) {
            legal.push_back(i + 1);
        }
        if (rank) {
            legal.push_back(i - 8);
        }
        if (irank) {
            legal.push_back(i + 8);
        }
        if (bl) {
            legal.push_back(i - 9);
        }
        if (br) {
            legal.push_back(i - 7);
        }
        if (fl) {
            legal.push_back(i + 7);
        }
        if (fr) {
            legal.push_back(i + 9);
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