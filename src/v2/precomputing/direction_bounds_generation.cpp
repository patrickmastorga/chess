#include <iostream>
#include <algorithm>

using namespace std;

int main()
{

    for (int i = 0; i < 64; i++) {
        int rank = i / 8;
        int file = i % 8;
        int irank = 7 - rank;
        int ifile = 7 - file;
        int fld = std::min(irank, file);
        int frd = std::min(irank, ifile);
        int bld = std::min(rank, file);
        int brd = std::min(rank, ifile);

        int b = i - 8 * rank;
        int f = i + 8 * irank;
        int l = i - 1 * file;
        int r = i + 1 * ifile;
        int bl = i - 9 * bld;
        int fr = i + 9 * frd;
        int br = i - 7 * brd;
        int fl = i + 7 * fld;

        cout << "{" << b << ", " << f << ", " << l << ", " << r << ", " << bl << ", " << fr << ", " << br << ", " << fl << "}," << endl;
    }
}