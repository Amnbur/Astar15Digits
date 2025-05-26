#include <cinttypes>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <algorithm>

/*
    设计思路
    初始OPEN表的唯一元素是棋盘初始状态，CLOSE表为空
    对于每个状态需要储存其g(n)、h(n)、f(n)和棋盘
    f(n) = g(n)+h(n)，取h(n)为每个数字到目标位置的曼哈顿距离，可以较快地找到目标
    每次将所有可达且不在OPEN表和CLOSE表中的状态加入到OPEN表，自身进入CLOSE表
    然后重排OPEN表，选择f(n)最小的一个继续循环，直到选取到目标状态
    
    //  查表方面的考虑：
        CLOSE表每次循环要查询多个状态，插入一个状态，需要提供快速查询的功能
        OPEN表每次循环要查找和删除一个最小元素，查询多个元素，需要快速查询
        综上所述，可以考虑使用哈希表存储两个表，使用小顶堆维护OPEN表的f(n)存在的偏序关系

*/

typedef unsigned Cost;

class Grid16 {
private:
    uint64_t data;
public:
    // 构造函数
    Grid16(uint64_t initialValue = 0) : data(initialValue) {}
// get & set & display
    uint8_t getCell(uint8_t index) const {
        return (data >> (index << 2)) & 0xF;
    }
    void setCell(uint8_t index, uint8_t value) {
        data &= 0xFFFFFFFFFFFFFFFFULL ^ (0x0FULL << (index << 2));
        data |= (0x0FULL & value) << (index << 2);
    }
    void displayLine(void) const {
        for (int i=0;i<15;++i)
            printf("%2u,",getCell(i));
        printf("%2u\n",getCell(15));
    }
    void displaySquare(void) const {
        int i=0;
        while (i<16){
            for (int j=0;j<3;++j)
                printf("%2u,",getCell(i++));
            printf("%2u\n",getCell(i++));
        }
    }
// cal
    inline uint8_t calDis(uint8_t index,uint8_t value);
    Cost calH(void){
        Cost h = 0;
        for (uint8_t i = 0;i<=15;++i)
            h += calDis(i,getCell(i));
        return h;
    }
// raw
    inline uint64_t getRawValue() const {
        return data;
    }
    inline uint64_t hash() const {
        return getRawValue();
    }
    void setRawValue(uint64_t value) {
        data = value;
    }
//  gen
    inline void swapCell(uint8_t id0,uint8_t id1);
    inline void swapCell(uint8_t id0,uint8_t id1,Grid16& s,uint64_t* &p);
    inline void swapCell10(uint8_t id,uint8_t zi,Grid16& s,uint64_t* &p);
    uint64_t* genStatus(uint64_t* buffer);  //  返回end
};

class Status{
public:
    Cost f = 0;
    Cost g = 0;
    Cost h = 0;
    //  16个格子，数字分别为1~14，两个空格
    //  目标状态: 1,2,3,4, 5,6,7,8, 9,a,b,c, d,e,0,0
    Grid16 stat;
    Grid16 lastStat = 0;
    //  每四位表示一个格子

    Status(){}
    Status(uint64_t value):stat(value){
        this -> h = stat.calH();
    }
    Status(std::vector<uint8_t> &values){
        stat = 0;
        uint8_t index=0;
        for (uint8_t v:values)
            stat.setCell(index++,v);
        this -> h = stat.calH();
    }
};

//  小顶堆需要greater比较
struct GreaterStatus {
    bool operator()(const Status* a, const Status* b) const {
        return a->f > b->f;  // 小顶堆
    }
};

#define MAX_SOLVE_LEN 400
const uint64_t targetStat = 0x00edcba987654321ULL;   // 1,2,3,4,5,6,7,8,9,10,11,12,13,14,0,0
typedef unsigned Cost;  //  这里可以使用更小的类型吗？
uint64_t solvePath[MAX_SOLVE_LEN];  //  保存路径
int solveLen = 0;
uint8_t swapCommand[MAX_SOLVE_LEN*2];  //  保存交换命令
int swapCommandLen = 0;

std::vector<Status*> openQueue; //  小顶堆
std::unordered_map<uint64_t,uint64_t> openList;  //  open表，表示拓展出来的元素，周围还有其他状态可能需要拓展
std::unordered_map<uint64_t,uint64_t> closeList; //  close表，其中的元素周围的所有状态都拓展过了
                                        //  两个表中尽存储哈希值和上一个状态的哈希值，以便节省空间和从最终状态回溯得到解
const char *configFileName = "solverConfig.ini";
const char *swapCommandFileName = "output.txt";
std::vector<uint8_t> initState;
const int cellRequired = 16;
int zeroCount=0;

// arg
bool argParser();
bool getArgs();

// test
int test();
int testGenTargetStatus();
int testSetCell();
int testSwapCell();

// display
int showSolve();
int showSolveSquare();
int showSolvePlainText();
int generateSwapCommand();
int showSwapCommand();

void heapFreeFunc();

int main(){
    atexit(heapFreeFunc);

    // return test();
    if (!getArgs())   return -1;

    uint64_t nextStat[32];  //  万一有很多个0
    uint64_t *sp = nextStat;

    openQueue.reserve(1024);
    openQueue.push_back(new Status(initState));
    openList.insert(std::pair(openQueue[0]->stat.hash(),0)); 

    //  比较符
    GreaterStatus comp;
    while(openQueue.size()){
        //  取队首元素，判断是否为目标状态，拓展周围状态
        auto lastStatP = openQueue[0];
        if (lastStatP->stat.hash() == targetStat){  
            //  找到目标状态，开始回溯解决路径
            solveLen = 0;
            solvePath[solveLen++] = lastStatP->stat.hash();
            uint64_t sh = lastStatP->lastStat.hash();
            
            auto p = closeList.find(sh);
            while (p != closeList.end()){
                solvePath[solveLen++] = sh;
                sh = p->second;
                if (sh == 0)    //  已经回溯到初始状态
                    break;
                p = closeList.find(sh);
            }
            //  打印路径
            return showSolve();
        }

        sp = openQueue[0]->stat.genStatus(nextStat);
        if (sp > nextStat && sp - nextStat <= 32){ //  产生了一些状态，下面在open表和close表中查询
            while (sp > nextStat){
                --sp;
                //  查表是否重复
                if (openList.count(*sp) == 0 && closeList.count(*sp) == 0){
                    //  没有重复，则加入open表和open堆
                    openList.insert(std::pair(*sp,lastStatP->stat.hash()));
                    Status* newStatP = new Status(*sp);
                    openQueue.push_back(newStatP);

                    //  设置g(n)、f(n)、父状态
                    newStatP->g = lastStatP->g + 1;
                //  h(n)在初始化时候计算了，见Status(uint64_t value)
                    newStatP->f = newStatP->g + newStatP->h;
                    newStatP->lastStat = lastStatP->stat;  //  设置父状态
                    
                    //  维护堆
                    std::push_heap(openQueue.begin(),openQueue.end(),comp);
                }
            }
        }
        //  取出队首元素加入到close表，从open表中删除
        openList.erase(openQueue[0]->stat.hash());
        closeList.insert(std::pair(openQueue[0]->stat.hash(),openQueue[0]->lastStat.hash()));
        std::pop_heap(openQueue.begin(),openQueue.end(),comp);
        openQueue.pop_back();
    }

    return 0;
}

//  假定最终状态是确定的1,2,3,4,5,6,7,8,9,10,11,12,13,14,0,0
uint8_t Grid16::calDis(uint8_t index,uint8_t value){
    if (value == 0u)
        return 0;
    uint8_t retv = 0;
    uint8_t ir,vr,ic,vc;
    value -= 1;  
    //  value=1时应该处于index=0的位置,变换后的value表示目标位置,index表示当前位置
    ir = index >> 2;
    vr = value >> 2;
    ic = index & 0x03;
    vc = value & 0x03;
    retv += (ir>=vr)?(ir-vr):(vr-ir);
    retv += (ic>=vc)?(ic-vc):(vc-ic);
    return retv;
}

void Grid16::swapCell(uint8_t id0,uint8_t id1){
    uint8_t tmp = this->getCell(id1);
    this->setCell(id1,getCell(id0));
    this->setCell(id0,tmp);
}
void Grid16::swapCell(uint8_t id0,uint8_t id1,Grid16& s,uint64_t* &p){
    uint8_t tmp = s.getCell(id1);
    if (tmp==0)
        swapCell10(id0,id1,s,p);return;
    if (id0 <= 0xFU && id1 <=0xFU){
        s.setCell(id1,getCell(id0));
        s.setCell(id0,tmp);
    }
}
void Grid16::swapCell10(uint8_t id,uint8_t zi,Grid16& s,uint64_t* &p){
    if (id <= 15 && this->getCell(id)){   //  没有越界并且不为0
            s.setCell(zi,getCell(id));  //  交换两个方格
            s.setCell(id,0);
            *p++ = s.getRawValue();
        }
}

uint64_t* Grid16::genStatus(uint64_t* buffer){
    //  找到0的位置
    uint8_t zeros[16];
    uint8_t zeroCount = 0;
    uint8_t id,i,zi;
    uint64_t *p = buffer;
    if (p == nullptr)
        return nullptr;

    Grid16 s;
    for (id=0;id<16;++id)  //  找到所有的0
        if (!this->getCell(id))
            zeros[zeroCount++] = id;
    for (i=0;i<zeroCount;++i){
        zi = zeros[i];

        id = zi-1;  //  左
        if ((id>>2) == (zi>>2)){ //  同一行
            s.data = this->data;
            swapCell(id,zi,s,p);
        }

        id = zi+1;  //  右  
        if ((id>>2) == (zi>>2)){
            s.data = this->data;
            swapCell(id,zi,s,p);
        }
        
        s.data = this->data;
        id = zi-4;  //  上
        swapCell(id,zi,s,p);

        s.data = this->data;
        id = zi+4;  //  下
        swapCell(id,zi,s,p);
    }
    return p;
}

bool argParser(){
    FILE* cf = fopen(configFileName,"r");
    char tc;
    int c=0;
    unsigned v;

    if (!cf)
        return false;
    if (fscanf(cf, "initState%c", &tc) <= 0 || tc != '=')
        goto PARSING_EXCEPTION;
    while (true){
        int ret;
        ret = fscanf(cf, "%u", &v);
        if (ret == EOF)
            break;
        if (ret == 0)
            fgetc(cf);
        else{
            tc = fgetc(cf); //  读一个分隔符
            initState.push_back(v);
            ++c;
            if (c >= cellRequired)
                break;
        }
    }
    if (0){
PARSING_EXCEPTION:
        fclose(cf);
        return false;
    }
    fclose(cf);
    return true;
}
bool getArgs(){
    initState.reserve(cellRequired);
    unsigned tv;
    if (!argParser()){
        printf("invalid config file.\n");
        printf("input initstate manually:\n");
        for (int i=0;i<cellRequired;++i){
            scanf("%d",&tv);
            initState.push_back(tv);
        }
    }
    if (initState.size() != std::size_t(cellRequired))
        return false;
    return true;
}

int showSolve(){
    showSolveSquare();
    generateSwapCommand();
    showSwapCommand();
    return 0;
}

int showSolvePlainText(){
    Grid16 s;
    for (int i=solveLen-1;i>=0;--i){
        s = Grid16(solvePath[i]);
        s.displayLine();
    }
    return 0;
}
int showSolveSquare(){
    Grid16 s;
    for (int i=solveLen-1;i>=0;--i){
        s = Grid16(solvePath[i]);
        s.displaySquare();
        puts("------------");
    }
    printf("%d steps.\n",solveLen-1);
    return 0;
}
int generateSwapCommand(){
    uint8_t diff[16];
    uint8_t diffCount;
    Grid16 *a,*b;
    for (int i=solveLen-1;i>0;--i){
        diffCount = 0;
        a = (Grid16*)&solvePath[i];
        b = (Grid16*)&solvePath[i-1];
        for (int j=0;j<16;++j){
            if (a->getCell(j) != b->getCell(j))
                diff[diffCount++] = j;
            if (diffCount >= 2)
                break;
        }
        if (diffCount == 2){
            swapCommand[swapCommandLen++] = diff[0];
            swapCommand[swapCommandLen++] = diff[1];
        }else{
            perror("[err] invalid swap");
        }
    }
    
    return 0;
}
int showSwapCommand(){
    for (int i=0;i<swapCommandLen;++i){
        printf("%u,",swapCommand[i]);
        printf("%u, ",swapCommand[++i]);
    }
    puts("");

    FILE* ofp = fopen(swapCommandFileName,"w");
    if (!ofp)
        puts("failed to open output file");
    else for (int i=0;i<swapCommandLen;++i){
        fprintf(ofp,"%u,",swapCommand[i]);
        fprintf(ofp,"%u,",swapCommand[++i]);
    }
    fclose(ofp);
    return 0;
}

int test(){
    return testSwapCell();
}

int testGenTargetStatus(){
    initState.assign(16,0);
    for (int i=0;i<14;++i){
        initState[i] = i+1;
    }
    initState[14] = 0;
    initState[15] = 0;
    Status s = Status(initState);
    printf("%llu\n",s.stat.hash());
    s.stat.displaySquare();
    return 0;
}

int testSetCell(){
    // getArgs();
    Status s(targetStat);
    Grid16 &g = s.stat;
    s.stat.displaySquare();
    puts("------------");
    for (int i=0;i<16;++i){
        g.setCell(i,i*7+3);
        g.displaySquare();
        puts("------------");
    }
    return 0;
}

int testSwapCell(){
    Status s(targetStat);
    Grid16 &g = s.stat;
    printf("initState\n");
    s.stat.displaySquare();
    uint8_t testCase[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    int i=1;
    uint8_t a,b;
    a=testCase[0];
    while (i<16){
        b=testCase[i];
        printf("swap %u,%u\n",a,b);
        g.swapCell(a,b);
        g.displaySquare();
        puts("------------");
        a=b;b=testCase[++i];
    }
    return 0;
}

void heapFreeFunc(){
    for (auto p:openQueue)
        free(p);
    openQueue.clear();
    openList.clear();
    closeList.clear();
}