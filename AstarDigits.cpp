#include <cinttypes>
#include <iostream>
#include <unordered_set>
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
typedef unsigned Cost;  //  这里可以使用更小的类型吗？

class Grid16 {
private:
    uint64_t data;
public:
    // 构造函数
    Grid16(uint64_t initialValue = 0) : data(initialValue) {}
// get & set
    uint8_t getCell(uint8_t index) const {
        return (data >> (index << 2)) & 0xF;
    }
    void setCell(uint8_t index, uint8_t value) {
        data &= ~(0xFULL << (index << 2));
        data |= (value & 0xF) << (index << 2);
    }
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
    inline void swapCell(uint8_t id,uint8_t zi,Grid16& s,uint64_t* p);
    uint64_t* Grid16::genStatus(uint64_t* buffer);  //  返回end

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
    Status(std::vector<uint8_t> values){
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

std::unordered_set<uint64_t> open;
std::unordered_set<uint64_t> close;
std::vector<uint8_t> initState;
const char *configFileName = "config.ini";

bool argParser();
const int cellRequired = 16;

int main(){
    initState.reserve(cellRequired);
    unsigned tv;
    int zeroCount=0;

    if (!argParser()){
        printf("invalid config file.\n");
        printf("input initstate manually:\n");
        for (int i=0;i<cellRequired;++i){
            scanf("%d",&tv);
            initState.push_back(tv);
        }
    }

    uint64_t nextStat[32];  //  万一有很多个0
    std::vector<Status*> openQueue; //  小顶堆

    openQueue.reserve(400);
    openQueue.push_back(new Status(initState));

    //  比较符
    GreaterStatus comp;
    while(openQueue.size()){
        
        std::push_heap(openQueue.begin(),openQueue.end(),comp);
    }

    return 0;
}

//  假定最终状态是确定的1,2,3,4,5,6,7,8,9,10,11,12,13,14,0,0
uint8_t Grid16::calDis(uint8_t index,uint8_t value){
    if (!value)
        return 0;
    uint8_t retv = 0;
    uint8_t ir,vr,ic,vc;
    ir = index >> 2;
    vr = value >> 2;
    ic = index & 0x03;
    vc = index & 0x03;
    retv += (ir>=vr)?(ir-vr):(vr-ir);
    retv += (ic>=vc)?(ic-vc):(vc-ic);
    return retv;
}

void Grid16::swapCell(uint8_t id,uint8_t zi,Grid16& s,uint64_t* p){
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

        s.data = this->data;
        id = zi-1;  //  左
        if ((id>>2) == (zi>>2)) //  同一行
            swapCell(id,zi,s,p);
        s.data = this->data;
        id = zi+1;  //  右
        if ((id>>2) == (zi>>2))
            swapCell(id,zi,s,p);
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

