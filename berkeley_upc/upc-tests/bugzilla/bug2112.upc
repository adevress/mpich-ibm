struct __ctype {
        unsigned __collate:8;
        unsigned __upper:8;
        unsigned __lower:8;
        unsigned __flags:20;
};
extern struct __ctype *__ctypemap;
static int __tolower(int _C) {
        return(_C == -1 ? -1 : ((__ctypemap + 1)[_C].__lower));
}

int main() {
}
