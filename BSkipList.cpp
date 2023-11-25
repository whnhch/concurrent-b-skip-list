#include <iostream>
#include <vector>
#include <stack>
#include <random>
#include <limits.h>
#include <time.h>
using namespace std;
class Block;

class Node
{
public:
    int value;
    Block *down; // Pointer to lower level block contains same value
    Node(int value, Block *down)
    {
        this->value = value;
        this->down = down;
    }
};

class Block
{
public:
    std::vector<Node *> vector;
    Block *next; // Pointer to the next block at the same level

    Block(Node *node, Block *next)
    {
        vector.push_back(node);
        // vector.resize(3); // minimum size of each block
        this->next = next;
    }

    Block(std::vector<Node *> vector, Block *next)
    {
        this->vector = vector;
        // vector.resize(3); // minimum size of each block
        this->next = next;
    }

    void print()
    {
        for (unsigned int i = 0; i < vector.size(); i++)
        {
            std::cout << vector[i]->value;
            if (vector[i]->down)
                std::cout << "(" << vector[i]->down->vector[0]->value << ")";
            std::cout << " ";
        }
        std::cout << "| ";
    }
};

class BSkipList
{
private:
    std::vector<Block *> levels; // Vector of head blocks from each level
    std::stack<Block *> getBlockStack(int value)
    {
        int lvl = levels.size() - 1;
        Block *current = levels[levels.size() - 1]; // starting from first block in higest level
        std::stack<Block *> blocks;                 // store the path
        Block *block = current;                     // keep track the place for value
        Node *prev;
        while (current)
        {
            bool found = false;
            // find a value greater than insert value
            for (unsigned int i = 0; i < current->vector.size(); i++)
            {
                if (value > current->vector[i]->value)
                { // go to next node
                    prev = current->vector[i];
                }
                else
                { // find the place
                    blocks.push((block));
                    current = prev->down;
                    lvl--;
                    block = current;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                // keep looking in next block
                if (current->next)
                {
                    current = current->next;
                    // last in current block
                    if (value < current->vector[0]->value)
                    {
                        blocks.push(block);
                        current = prev->down;
                    }
                }
                else // last in this level
                    blocks.push(current);
                current = prev->down;
            }
            block = current;
        }
        return blocks;
    }

public:
    int r = 1;
    const float P_FACTOR = 0.25;
    // const int MAX_LEVEL = 32;
    // const float P_FACTOR = 0.25;
    // static std::random_device rd; // obtain a random number from hardware
    // static std::mt19937 gen(rand()); // seed the generator
    // static std::uniform_real_distribution<> distr(0, 1); // define the range
    BSkipList()
    {
        Block *block = new Block(new Node(INT_MIN, nullptr), nullptr); // negative infinity block
        levels.push_back(block);
    }

    ~BSkipList()
    {
        // Destructor to free memory
        // ... (cleanup logic here)
    }

    void insert(int value)
    {
        srand(time(NULL)); // initialize random seed
        std::stack<Block *> blocks = getBlockStack(value);
        Block *lower = nullptr;
        // building block from botton
        while (!blocks.empty())
        {
            bool inserted = false;
            Block *block = blocks.top();
            blocks.pop();
            for (unsigned int i = 0; i < block->vector.size(); i++)
            {
                if (block->vector[i]->value > value)
                { // in the middle of the vector
                    // (static_cast<float>(rand()) / RAND_MAX) < P_FACTOR)
                    if ((static_cast<float>(rand()) / RAND_MAX) < P_FACTOR)
                    { // tail
                        //r = r + rand();
                        block->vector.insert(block->vector.begin() + i, new Node(value, lower));
                        return;
                    }
                    else
                    { // head
                        r++;
                        // split and shrink block
                        std::vector<Node *> right;
                        right.push_back(new Node(value, lower));
                        for (unsigned int j = i; j < block->vector.size(); j++)
                            right.push_back(block->vector[j]);
                        block->vector.resize(i);
                        Block *rightBlock = new Block(right, block->next);
                        block->next = rightBlock;
                        // new level
                        if (blocks.empty())
                        {
                            Block *up = new Block(new Node(INT_MIN, block), nullptr);
                            up->vector.push_back(new Node(value, block->next));
                            levels.push_back(up);
                        }
                        inserted = true;
                        lower = block->next;
                        break;
                    }
                }
            }
            if (!inserted)
            {
                // at the end of the vector
                if ((static_cast<float>(rand()) / RAND_MAX) < P_FACTOR)
                { // tail
                    r = r + 1;
                    block->vector.push_back(new Node(value, lower));
                    return;
                }
                else
                { // head
                    r = r + rand();
                    Block *newBlock = new Block(new Node(value, lower), block->next);
                    block->next = newBlock;
                    // new level
                    if (blocks.empty())
                    {
                        Block *up = new Block(new Node(INT_MIN, block), nullptr);
                        up->vector.push_back(new Node(value, newBlock));
                        levels.push_back(up);
                    }
                    lower = newBlock;
                }
            }
        }
    }

    void remove(int value)
    {
        std::stack<Block *> blocks = getBlockStack(value);
        Block *current;
        Block *block;
        vector<Block *> update;
        Block *curr = nullptr;
        bool flag = false;
        for (int i = levels.size() - 1; i >= 0; i--)
        {
            Block *pre = nullptr;
            curr = levels[i];
            while (curr)
            {
                for (int j = 0; j < curr->vector.size(); j++)
                {
                    if (curr->vector[j]->value == value)
                    {
                        if (pre)
                        {
                            flag = true;
                            update.push_back(pre);
                            //cout << pre->vector[0]->value << "pre" << endl;
                        }
                        break;
                    }
                }
                if (flag)
                {
                    flag = false;
                    break;
                }

                pre = curr;
                curr = curr->next;
            }
        }

        // for (int i = 0; i < update.size(); i++)
        // {
        //     cout << update[i]->vector[0]->value << "update" << endl;
        //     if (update[i]->next)
        //     {
        //         cout << update[i]->next->vector[0]->value << "update next" << endl;
        //         if(update[i]->next->vector.size() > 1){
        //             cout << update[i]->next->vector[1]->value << "test" << endl;
        //         }
        //     }
        // }
        int x = 0;
        while (!blocks.empty())
        {
            block = blocks.top();
            blocks.pop();

            for (unsigned int i = 0; i < block->vector.size(); i++)
            {
                if (block->vector[i]->value == value)
                {
                    Block *downBlock = block->vector[i]->down;
                    block->vector.erase(block->vector.begin() + i);

                    while (downBlock != nullptr)
                    {
                        current = downBlock->vector[0]->down;
                        downBlock->vector.erase(downBlock->vector.begin());
                        if(!downBlock->vector.empty()){
                            update[x]->vector.insert(update[x]->vector.end(), downBlock->vector.begin(),downBlock->vector.end());
                            update[x]->next = update[x]->next->next;
                            x++;
                        }else{
                            update[x]->next = update[x]->next->next;
                            x++;
                        }

                        downBlock = current;
                    }
                }
            }
        }
    }

    void print_list(){
        Block* curr;
        for (int i = levels.size() - 1; i >= 0; i--)
        {
            Block *pre = nullptr;
            curr = levels[i];
            while (curr)
            {
                for (int j = 0; j < curr->vector.size(); j++)
                {

                    cout << curr->vector[j]->value << " ";
                    if(curr->vector[j]->down){
                        cout << "(" << curr->vector[j]->down->vector[0]->value << ")";
                    }
                }
                curr = curr->next;
                cout << "|";
            }
            cout << " " << endl;
        }
    }

    void print()
    {
        for (unsigned int i = levels.size() - 1; i >= 0; i--)
        {
            Block *current = levels[i];
            while (current)
            {
                current->print();
                current = current->next;
            }
            std::cout << std::endl;
        }
    }

    bool search(int key)
    {
        std::vector<Node *>::iterator it;
        Node *node;
        Node *prev_node;
        Block *block = levels[levels.size() - 1];

        

        while (block)
        {
            for (it = block->vector.begin(); it != block->vector.end(); ++it)
            {
                node = *it;
                if (node->value < key)
                {
                    prev_node = node;
                    if (node == *std::prev(block->vector.end()))
                    {
                        block = block->next;
                        break;
                    }
                    else
                    {
                        continue;
                    }
                }
                else if (node->value == key)
                {
                    return true;
                }
                else if (key < node->value)
                {
                    block = prev_node->down;
                    break;
                }
                // else if (i == 0) {return false;}
            }
        }
        // }
        return false;
    }

    std::vector<bool> range_query(int _start_key, int _end_key) {
        int start_key = _start_key;
        int end_key = _end_key;

        std::vector<Node*>::iterator it;
        std::vector<bool> output;
        bool value = false;
        
        Node* node;
        Node* prev_node;

        //first find the start key value
        Block* block = levels[levels.size() - 1];

        while(true){
            while(block) {
                if(value){break;}
                
                for (it = block->vector.begin(); it != block->vector.end(); ++it) {
                    node = *it;
                    if(node->value < start_key){
                        prev_node = node; 
                        if(node==*std::prev(block->vector.end())){
                            block = block->next;
                            break;
                        }
                        else{continue;}
                    }
                    else if(node->value == start_key) { 
                        value = true;
                        break;
                    }
                    else if (start_key < node->value){
                        block = prev_node->down; 
                        break;
                    } 
                }
            }
            output.push_back(value);
            start_key+=1;
            
            if(value){++it; break;}
            //prevent when the first key is not found
            //if the first key is not found set the next key is first key
            else if(start_key == end_key){break;}
            else{
                block = levels[levels.size() - 1];
            }
        }

        //propagates to next node until the key is below than end_key
        //if there is no more blocks the break the loop.
        int cur_key = start_key;

        while(block){ 
            if(cur_key >= end_key) break;

            if(it == block->vector.end()) {
                block = block->next;
                if(block==NULL){break;}
                it = block->vector.begin();
            }

            node = *it;
            if(node->value == cur_key){
                value = true;
                ++cur_key;
                ++it;
            }
            else if(node->value > cur_key){
                value = false;
                ++cur_key;
            }

            else{
                value = false;
                ++it;
            }
            output.push_back(value);
        }

        //if the end block reached before finding all keys
        //fill the output with false
        if(cur_key<end_key){
            for(;cur_key<end_key;cur_key++){
                output.push_back(false);
            }
        }
        return output;
    }
};

void test_search(BSkipList list)
{
    // Test Search
    std::cout << "==========================" << std::endl;
    std::cout << "Test for search" << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << "Search 1: " << std::boolalpha << list.search(1) << std::endl;
    std::cout << "Search 3: " << std::boolalpha << list.search(3) << std::endl;
    std::cout << "Search 4: " << std::boolalpha << list.search(4) << std::endl;
    std::cout << "Search 10: " << std::boolalpha << list.search(10) << std::endl;
    std::cout << "Search 5: " << std::boolalpha << list.search(5) << std::endl;
    std::cout << "Search 14: " << std::boolalpha << list.search(5) << std::endl;
    std::cout << "Search 2: " << std::boolalpha << list.search(5) << std::endl;
}

void test_range_query(BSkipList list, int start, int end)
{
    // Test Range Query
    std::vector<bool> rq_output = list.range_query(start, end);
    std::vector<bool>::iterator it;
    int i;

    std::cout << "==========================" << std::endl;
    std::cout << "Test for range search from " << start << " to " << end << std::endl;
    std::cout << "==========================" << std::endl;

    for (it = rq_output.begin(), i = start; it != rq_output.end() && i < end; it++, i++)
    {
        std::cout << "Search " << i << ": " << std::boolalpha << *it << std::endl;
    }
}

int main()
{
    BSkipList list;
    list.insert(1);
    list.insert(10);
    list.insert(3);
    list.insert(2);
    list.insert(6);
    list.insert(11);
    list.insert(7);
    list.print_list();
    list.remove(7);
    cout << "=========="<<endl;
    // list.insert(8);
    // list.insert(-1);
    // std::cout << list.search(-1) << std::endl;
    // std::cout << list.search(-2) << std::endl;
    // std::cout << list.search(11) << std::endl;
    list.print_list();
    return 0;
}