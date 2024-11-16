#include <conio.h>
#include <windows.h>
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <utility>
#include <map>
#include <algorithm>
#include <iomanip>
#include <cctype>
#include <io.h>
#include <time.h> //�ð�
#include <thread> //�Ʒ����� ������ ���
#include <functional>
#include <condition_variable>
#include <mutex>
#include <queue>

using namespace std;

typedef pair<string, int> KeyValuePair;

void showMainMenu();
void drawMainScreen();
void drawCursor();
void showMenuOptions();
void handleEnterKey();
void modeMenuScreen();
void tryProgram(int dan);
void showInfo();
void exitProgram();
void waitForEnter();
void wordCount();
void printWordCounts();

int currentMenu = 1; // ���� ���õ� �޴�
//input ����, map_file ����, result ���� ����� ��� �����ϱ�
string path_in = "./input_file/input.txt"; //�������� ���
string path_map_out = "./map_file/"; //map, solt ���� ���� ���
string path_result = "./result_file/result.txt"; //���� ���� ���� ���

int block_size = 100000; //���ϳ����� ���� (�� ������ �ִ� �� ��)
int word_size = block_size * 10;
int thread_size = 2;

double duration = 0;
int minaute = 0;
int sec = 0;
time_t start, finish;

typedef pair<string, int> KeyValuePair;

//--------------------------------  ������ Ǯ  -------------------------------------------------
namespace ThreadPool {
    class ThreadPool {
    public:
        ThreadPool(size_t num_threads);
        ~ThreadPool();

        // job �� �߰��Ѵ�.
        void EnqueueJob(std::function<void()> job);

    private:
        // �� Worker �������� ����.
        size_t num_threads_;
        // Worker �����带 �����ϴ� ����.
        std::vector<std::thread> worker_threads_;
        // ���ϵ��� �����ϴ� job ť.
        std::queue<std::function<void()>> jobs_;
        // ���� job ť�� ���� cv �� m.
        std::condition_variable cv_job_q_;
        std::mutex m_job_q_;

        // ��� ������ ����
        bool stop_all;

        // Worker ������
        void WorkerThread();
    };

    ThreadPool::ThreadPool(size_t num_threads)
        : num_threads_(num_threads), stop_all(false) {
        worker_threads_.reserve(num_threads_);
        for (size_t i = 0; i < num_threads_; ++i) {
            worker_threads_.emplace_back([this]() { this->WorkerThread(); });
        }
    }

    void ThreadPool::WorkerThread() {
        while (true) {
            std::unique_lock<std::mutex> lock(m_job_q_);
            cv_job_q_.wait(lock, [this]() { return !this->jobs_.empty() || stop_all; });
            if (stop_all && this->jobs_.empty()) {
                return;
            }

            // �� ���� job �� ����.
            std::function<void()> job = std::move(jobs_.front());
            jobs_.pop();
            lock.unlock();

            // �ش� job �� �����Ѵ� :)
            job();
        }
    }

    ThreadPool::~ThreadPool() {
        stop_all = true;
        cv_job_q_.notify_all();

        for (auto& t : worker_threads_) {
            t.join();
        }
    }

    void ThreadPool::EnqueueJob(std::function<void()> job) {
        if (stop_all) {
            throw std::runtime_error("ThreadPool ��� ������");
        }
        {
            std::lock_guard<std::mutex> lock(m_job_q_);
            jobs_.push(std::move(job));
        }
        cv_job_q_.notify_one();
    }

}  // namespace ThreadPool

//--------------------------------  ���� ����  -------------------------------------------------
//�������� ���� ������
vector<string> file_splite(string path, string out_path, int block_size) {
    string line;
    vector<string> path_buffer;

    int line_count = 0;
    int block_count = 0;
    bool finish = true;

    ifstream file_read(path);

    if (file_read.is_open()) {
        while (finish) {
            string tp = out_path + to_string(block_count) + ".txt"; //�߰� ���ϸ�
            path_buffer.push_back(tp);
            ofstream file_wr(tp);

            if (file_wr.is_open()) {
                while (line_count < block_size) {
                    if (getline(file_read, line)) {
                        file_wr << line << "\n";
                        line_count++;
                    }
                    else {
                        finish = false;
                        break;
                    }
                }

                //�� ���� ������
                file_wr.close();
                block_count++;
                line_count = 0;
            }
        }

    }
    else {
        cerr << "�������� ���� ����!!!" << path << endl;
        file_read.close();
        return path_buffer;
    }

    cout << "�������� ���� �Ϸ�!!!\n" << endl;
    file_read.close();
    return path_buffer;
}

//----------------------------------- �ʸ���� -------------------------------------------------
//���� �б�
vector<string> readFile(string filePath) {
    ifstream file_read(filePath);
    string line;
    vector<string> buffer;

    if (file_read.is_open()) {
        while (getline(file_read, line)) {
            buffer.push_back(line);
            //cout << line << endl;
        }

        file_read.close();
        return buffer;
    }
    else {
        cerr << "Unable to open file: " << filePath << endl;
        return buffer;
    }
}

//�ܾ� ���ĺ��� �����
string removePunctuation(string word) {
    string in_word = word;
    string result;

    for (char ch : in_word) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
            result += tolower(ch);
        }
        //cout << ch << endl;
    }
    return result;
}

//���� ������ �ڸ���
vector<string> split(string str, char Delimiter) {
    istringstream iss(str);
    string buffer;

    vector<string> result;

    while (getline(iss, buffer, Delimiter)) {
        result.push_back(removePunctuation(buffer));
    }

    return result;
}

//�� �����
void make_map(string re_f) {
    vector<string> text_line = readFile(re_f);

    if (!text_line.empty()) {
        ofstream file_wr(re_f);
        if (file_wr.is_open()) {
            for (int i = 0; i < text_line.size(); i++) {
                vector<string> text = split(text_line[i], ' ');

                for (int j = 0; j < text.size(); j++) {
                    if (!text[j].empty()) {
                        file_wr << text[j] + " " + "1" + "\n";
                    }
                }
            }

        }
        else {
            cout << "loss 1 file to map" << endl;
        }
        file_wr.close();
    }
    return;
}

//---------------------------------- �� �� ���� -------------------------------------------------
bool comp(string s1, string s2) {
    return s1 < s2;   // ���� ��
}

void solt_map(string re_f) {
    vector<string> map_key;
    string line;

    ifstream file_read(re_f);

    if (file_read.is_open()) {
        while (getline(file_read, line)) {
            map_key.push_back(line);
        }
        file_read.close();

        sort(map_key.begin(), map_key.end(), comp);

        ofstream file_wr(re_f);
        if (file_wr.is_open()) {
            for (int i = 0; i < map_key.size(); i++) {
                file_wr << map_key[i] + "\n";
            }
            file_wr.close();
        }

    }
    return;
}
//-------------------------------------- ���ེ -------------------------------------------------
void reduce(string path_map_out, string result_path) {
    vector<KeyValuePair> output;
    string line;

    string currentWord;
    int currentCount = 0;

    ifstream map_read(path_map_out);

    if (map_read.is_open()) {
        while (getline(map_read, line)) {
            istringstream iss(line);// �ڷ����� �°� �о� ��
            string word;
            int count;

            iss >> word >> count;

            if (word == currentWord) {
                //���ӵ� �ܾ� ���� ī��Ʈ
                currentCount += count;
            }
            else {
                // ���ӵ� �ܾ ���� �� ���� ���ӵǾ��� �ܾ� ī��Ʈ ����
                if (!currentWord.empty()) {
                    output.push_back(make_pair(currentWord, currentCount));
                }
                currentWord = word;
                currentCount = count;
            }
        }
        output.push_back(make_pair(currentWord, currentCount));
        map_read.close();
    }

    ofstream file_wr(result_path);
    if (file_wr.is_open()) {
        for (const auto& entry : output) {
            file_wr << entry.first + " " + to_string(entry.second) + "\n";
        }

        file_wr.close();
    }
    return;
}

//----------------------------------------- �ܺ� ���� -------------------------------------------
vector<string> soit_all_file(vector<string> map_path, string path_map_out, int max_size) {
    vector<string> map_path_buffer = map_path;

    string line;
    string line02;
    int line_count = 0;

    bool who_is_small = true;
    bool finish = true;
    bool finish02 = true;

    int i = 0;
    while ((i + 1) < map_path_buffer.size()) { //!(i + 1 >= map_path_buffer.size())
        ifstream file_read(map_path_buffer[i]);
        ifstream file_read02(map_path_buffer[i + 1]);

        if (file_read.is_open() && file_read02.is_open()) {
            string tp = path_map_out + to_string(map_path_buffer.size()) + ".txt";
            string str_buffer;
            map_path_buffer.push_back(tp);
            ofstream file_wr(tp);

            getline(file_read02, line02);
            while (finish || finish02) {
                if (who_is_small) {
                    if (finish) {
                        if (!getline(file_read, line)) {
                            finish = false;
                        }
                    }
                }
                else {
                    if (finish02) {
                        if (!getline(file_read02, line02)) {
                            finish02 = false;
                        }
                    }
                }

                if (finish) {
                    if (finish02) {
                        if (line < line02) {
                            str_buffer += line + "\n";
                            who_is_small = true;
                        }
                        else {
                            str_buffer += line02 + "\n";
                            who_is_small = false;
                        }
                    }
                    else {
                        str_buffer += line + "\n";
                        who_is_small = true;
                    }
                }
                else {
                    if (finish02) {
                        str_buffer += line02 + "\n";
                        who_is_small = false;
                    }
                }
                line_count++;

                if ((line_count >= max_size) || ((!finish) && (!finish02))) {
                    file_wr << str_buffer;
                    str_buffer.clear();
                    line_count = 0;
                }

            }

            //while�� �ʱ�ȭ
            i += 2;
            line_count = 0;

            who_is_small = true;
            finish = true;
            finish02 = true;

            file_read.close();
            file_read02.close();
            file_wr.close();
        }
        else {
            file_read.close();
            file_read02.close();
            return map_path_buffer;
            //break;
        }
    }

    return map_path_buffer;
}
//--------------------------------------- ���� ������ ����� ----------------------------------
void remove_befor_map_file(string path_map_out) {
    string befor_path_all = path_map_out + "*.*";
    struct _finddata_t fd;
    intptr_t handle;

    vector<string> file_list; //���� ���

    if ((handle = _findfirst(befor_path_all.c_str(), &fd)) == -1L)
        cout << "No file in directory!" << endl;

    while (_findnext(handle, &fd) == 0) {
        string tn_p = path_map_out + fd.name;
        file_list.push_back(tn_p); //���� ��� ����
    }
    _findclose(handle);

    for (size_t i = 1; i < file_list.size(); ++i) { //ù ������ ������ �ƴ�
        const auto& filePath = file_list[i];
        if (std::remove(filePath.c_str()) != 0) {
            std::perror(("Error deleting file: " + filePath).c_str());
        }
        else {
            std::cout << "File successfully deleted: " << filePath << '\n';
        }
    }
    file_list.clear();

}

//---------------------------------------- ������� �Լ� -----------------------------------------
void thread_make_map(string re_f, vector<bool>& thread_working_cheak,int thread_index, mutex& m) {
    vector<string> text_line = readFile(re_f);

    if (!text_line.empty()) {
        ofstream file_wr(re_f);
        if (file_wr.is_open()) {
            for (int i = 0; i < text_line.size(); i++) {
                vector<string> text = split(text_line[i], ' ');

                for (int j = 0; j < text.size(); j++) {
                    if (!text[j].empty()) {
                        file_wr << text[j] + " " + "1" + "\n";
                    }
                }
            }

        }
        else {
            cout << "loss 1 file to map" << endl;
        }
        file_wr.close();
    }

    m.lock();
    thread_working_cheak[thread_index] = true;
    m.unlock();

    return;
}

void thread_solt_map(string re_f, vector<bool>& thread_working_cheak, int thread_index, mutex& m) {
    vector<string> map_key;
    string line;

    ifstream file_read(re_f);

    if (file_read.is_open()) {
        while (getline(file_read, line)) {
            map_key.push_back(line);
        }
        file_read.close();

        sort(map_key.begin(), map_key.end(), comp);

        ofstream file_wr(re_f);
        if (file_wr.is_open()) {
            for (int i = 0; i < map_key.size(); i++) {
                file_wr << map_key[i] + "\n";
            }
            file_wr.close();
        }

    }

    m.lock();
    thread_working_cheak[thread_index] = true;
    m.unlock();
    return;
}

void thread_reduce(string path_map_out, string result_path, vector<bool>& thread_working_cheak, int thread_index, mutex& m) {
    vector<KeyValuePair> output;
    string line;

    string currentWord;
    int currentCount = 0;

    ifstream map_read(path_map_out);

    if (map_read.is_open()) {
        while (getline(map_read, line)) {
            istringstream iss(line);// �ڷ����� �°� �о� ��
            string word;
            int count;

            iss >> word >> count;

            if (word == currentWord) {
                //���ӵ� �ܾ� ���� ī��Ʈ
                currentCount += count;
            }
            else {
                // ���ӵ� �ܾ ���� �� ���� ���ӵǾ��� �ܾ� ī��Ʈ ����
                if (!currentWord.empty()) {
                    output.push_back(make_pair(currentWord, currentCount));
                }
                currentWord = word;
                currentCount = count;
            }
        }
        output.push_back(make_pair(currentWord, currentCount));
        map_read.close();
    }

    ofstream file_wr(result_path);
    if (file_wr.is_open()) {
        for (const auto& entry : output) {
            file_wr << entry.first + " " + to_string(entry.second) + "\n";
        }

        file_wr.close();
    }

    m.lock();
    thread_working_cheak[thread_index] = true;
    m.unlock();
    return;
}

//�׽�Ʈ ��
void thread_reduce02(string path_map_out, string result_path, vector<bool>& thread_working_cheak, int thread_index, mutex& m) {
    string currentWord;
    int currentCount = 0;

    vector<string> text_line = readFile(path_map_out);
    if (!text_line.empty()) {
        ofstream file_wr(result_path);
        if (file_wr.is_open()) {
            for (int i = 0; i < text_line.size(); i++) {
                istringstream iss(text_line[i]);// �ڷ����� �°� �о� ��
                string word;
                int count;

                iss >> word >> count;
                

                if (word == currentWord) {
                    //���ӵ� �ܾ� ���� ī��Ʈ
                    currentCount += count;
                }
                else {
                    // ���ӵ� �ܾ ���� �� ���� ���ӵǾ��� �ܾ� ī��Ʈ ����
                    if (!currentWord.empty()) {
                        file_wr << currentWord + " " + to_string(currentCount) + "\n";
                    }
                    currentWord = word;
                    currentCount = count;
                }
            }
            file_wr << currentWord + " " + to_string(currentCount) + "\n";

        }
        else {
            cout << "loss 1 file to reduce" << endl;
        }
        file_wr.close();
    }

    m.lock();
    thread_working_cheak[thread_index] = true;
    m.unlock();
    return;
}

int find_available_thread_index(vector<bool>& thread_working_cheak) {
    for (int i = 0; i < thread_working_cheak.size(); i++) {
        if (thread_working_cheak[i]) {
            return i;
        }
    }
    return 100;
}

//------------------------------------------ ���� ������ -----------------------------------------

void normal_mode() {
    //������ ������ map ��� ���� �����
    remove_befor_map_file(path_map_out);
    start = time(NULL);
    //�������� ����
    cout << "�������� ���� ����" << endl;
    vector<string> map_path_list = file_splite(path_in, path_map_out, block_size);

    if (map_path_list.size() >= 1) {
        //�� �������� �� ȭ ��Ű��
        cout << "�� ���� ����" << endl;
        for (int i = 0; i < map_path_list.size(); i++) {
            make_map(map_path_list[i]);
        }
        cout << "�� ���� �Ϸ�!!!\n" << endl;

        //�� �� ����
        cout << "�� ���� ����" << endl;
        for (int i = 0; i < map_path_list.size(); i++) {
            solt_map(map_path_list[i]);
        }
        cout << "�� ���� �Ϸ�!!!\n" << endl;

        //�ܺ� ����
        cout << "�ܺ����� ����" << endl;
        vector<string> map_path_final = soit_all_file(map_path_list, path_map_out, word_size);
        if (map_path_final.size() != map_path_list.size()) {
            map_path_list.clear();
        }

        //������� ���� ����
        for (size_t i = 0; i < map_path_final.size() - 1; ++i) {
            const auto& filePath = map_path_final[i];
            if (std::remove(filePath.c_str()) != 0) {
                std::perror(("Error deleting file: " + filePath).c_str());
            }
            else {
                std::cout << "File successfully deleted: " << filePath << '\n';
            }
        }
        cout << "�ܺ����� �Ϸ�!!!\n" << endl;
        string fianl_path = map_path_final[map_path_final.size() - 1];
        map_path_final.clear();

        //���ེ
        cout << "������ ���ེ ����" << endl;
        reduce(fianl_path, path_result);
    }

    cout << "Finsh!!!\n" << endl;
    finish = time(NULL);
    duration = (double)(finish - start);
    minaute = duration / 60;
    sec = (int)duration % 60;

    printWordCounts();
    printf("���ð� : %d�� %d��\n", minaute, sec);
    waitForEnter();
}

void middle_reduce_mode() {
    //������ ������ map ��� ���� �����
    remove_befor_map_file(path_map_out);
    start = time(NULL);
    //�������� ����
    cout << "�������� ���� ����" << endl;
    vector<string> map_path_list = file_splite(path_in, path_map_out, block_size);

    if (map_path_list.size() >= 1) {
        //�� �������� �� ȭ ��Ű��
        cout << "�� ���� ����" << endl;
        for (int i = 0; i < map_path_list.size(); i++) {
            make_map(map_path_list[i]);
        }
        cout << "�� ���� �Ϸ�!!!\n" << endl;

        //�� �� ����
        cout << "�� ���� ����" << endl;
        for (int i = 0; i < map_path_list.size(); i++) {
            solt_map(map_path_list[i]);
        }
        cout << "�� ���� �Ϸ�!!!\n" << endl;

        //�߰� ���ེ
        cout << "�� �߰� ���ེ ����" << endl;
        for (int i = 0; i < map_path_list.size(); i++) {
            reduce(map_path_list[i], map_path_list[i]);
        }
        cout << "�� �߰� ���ེ �Ϸ�!!!\n" << endl;

        //�ܺ� ����
        cout << "�ܺ����� ����" << endl;
        vector<string> map_path_final = soit_all_file(map_path_list, path_map_out, word_size);
        if (map_path_final.size() != map_path_list.size()) {
            map_path_list.clear();
        }

        //������� ���� ����
        for (size_t i = 0; i < map_path_final.size() - 1; ++i) {
            const auto& filePath = map_path_final[i];
            if (std::remove(filePath.c_str()) != 0) {
                std::perror(("Error deleting file: " + filePath).c_str());
            }
            else {
                std::cout << "File successfully deleted: " << filePath << '\n';
            }
        }
        cout << "�ܺ����� �Ϸ�!!!\n" << endl;
        string fianl_path = map_path_final[map_path_final.size() - 1];
        map_path_final.clear();

        //���ེ
        cout << "������ ���ེ ����" << endl;
        reduce(fianl_path, path_result);
    }

    cout << "Finsh!!!\n" << endl;
    finish = time(NULL);
    duration = (double)(finish - start);
    minaute = duration / 60;
    sec = (int)duration % 60;

    printWordCounts();
    printf("���ð� : %d�� %d��\n", minaute, sec);
    waitForEnter();
}

//Ȯ������ ������ ���
void middle_reduce_thread_mode() {
    //������ ������ map ��� ���� �����
    remove_befor_map_file(path_map_out);

    cout << "���� ���� ���� ����" << endl;
    start = time(NULL);

    //�������� ����
    vector<string> map_path_list = file_splite(path_in, path_map_out, block_size);

    //������ ���� ����
    vector<thread> threads;
    int k = 0;
    int finish_work_count = 0;
    
    if (map_path_list.size() >= 1) {
        //�� ���� �� ȭ
        cout << "�� ���� ����" << endl;
        while (!(finish_work_count == map_path_list.size())) {
            if (threads.size() < thread_size) { //�����尡 �Ѱ�ġ�� �ƴ� ��
                if (k < map_path_list.size()) {
                    threads.push_back(thread(make_map, map_path_list[k]));
                    k++;
                }
                else { //���̻� �߰��� �۾��� ���� ��
                    if (threads[0].joinable()) {
                        threads[0].join();
                        if (threads.size() != 1)
                            threads.erase(threads.begin());
                        finish_work_count++;
                    }
                }
            }
            else { //�����尡 �Ѱ�ġ ��ŭ �۵����� ��
                if (threads[0].joinable()) {
                    threads[0].join();
                    if (threads.size() != 1)
                        threads.erase(threads.begin());
                    finish_work_count++;
                }
            }
        }
        k = 0;
        finish_work_count = 0;
        threads.clear();
        cout << "�� ���� �Ϸ�!!!\n" << endl;

        cout << "�� ���� ����" << endl;
        //�� �� ����
        while (!(finish_work_count == map_path_list.size())) {
            if (threads.size() < thread_size) {
                if (k < map_path_list.size()) {
                    threads.push_back(thread(solt_map, map_path_list[k]));
                    k++;
                }
                else { //���̻� �߰��� �۾��� ���� ��
                    if (threads[0].joinable()) {
                        threads[0].join();
                        if (threads.size() != 1)
                            threads.erase(threads.begin());
                        finish_work_count++;
                    }
                }
            }
            else {
                if (threads[0].joinable()) {
                    threads[0].join();
                    if (threads.size() != 1)
                        threads.erase(threads.begin());
                    finish_work_count++;
                }
            }
        }
        k = 0;
        finish_work_count = 0;
        threads.clear();
        cout << "�� ���� �Ϸ�!!!\n" << endl;

        cout << "�� �߰� ���ེ ����" << endl;
        //�� �� �߰� ���ེ
        while (!(finish_work_count == map_path_list.size())) {
            if (threads.size() < thread_size) {
                if (k < map_path_list.size()) {
                    threads.push_back(thread(reduce, map_path_list[k], map_path_list[k]));
                    k++;
                }
                else { //���̻� �߰��� �۾��� ���� ��
                    if (threads[0].joinable()) {
                        threads[0].join();
                        if (threads.size() != 1)
                            threads.erase(threads.begin());
                        finish_work_count++;
                    }
                }
            }
            else {
                if (threads[0].joinable()) {
                    threads[0].join();
                    if (threads.size() != 1)
                        threads.erase(threads.begin());
                    finish_work_count++;
                }
            }
        }
        k = 0;
        finish_work_count = 0;
        threads.clear();
        cout << "�� �߰� ���ེ �Ϸ�!!!\n" << endl;

        cout << "�ܺ����� ���� " << endl;
        //�ܺ� ����
        vector<string> map_path_final = soit_all_file(map_path_list, path_map_out, word_size);
        if (map_path_final.size() != map_path_list.size()) {
            map_path_list.clear();
        }

        //������� ���� ����
        for (size_t i = 0; i < map_path_final.size() - 1; ++i) {
            const auto& filePath = map_path_final[i];
            if (std::remove(filePath.c_str()) != 0) {
                std::perror(("Error deleting file: " + filePath).c_str());
            }
            else {
                std::cout << "File successfully deleted: " << filePath << '\n';
            }
        }
        string fianl_path = map_path_final[map_path_final.size() - 1];
        map_path_final.clear();
        cout << "�ܺ����� �Ϸ�!!! \n" << endl;

        cout << "������ ���ེ ����" << endl;
        //���ེ
        reduce(fianl_path, path_result);
    }

    cout << "Finish!!!\n" << endl;
    finish = time(NULL);
    duration = (double)(finish - start);
    minaute = duration / 60;
    sec = (int)duration % 60;

    printWordCounts();
    printf("���ð� : %d�� %d��\n", minaute, sec);
    waitForEnter();
}

//������ Ǯ ���
void middle_reduce_thread_mode02() {
    //������ ������ map ��� ���� �����
    remove_befor_map_file(path_map_out);

    cout << "���� ���� ���� ����" << endl;
    start = time(NULL);

    //�������� ����
    vector<string> map_path_list = file_splite(path_in, path_map_out, block_size);

    if (map_path_list.size() >= 1) {
        //�� ���� �� ȭ
        cout << "�� ���� ����" << endl;
        {
            ThreadPool::ThreadPool pool(thread_size);
            for (int i = 0; i < map_path_list.size(); i++) {
                pool.EnqueueJob([&map_path_list, i]() { make_map(map_path_list[i]); });
            }
        }
        cout << "�� ���� �Ϸ�!!!\n" << endl;

        cout << "�� ���� ����" << endl;
        //�� �� ����
        {
            ThreadPool::ThreadPool pool(thread_size);
            for (int i = 0; i < map_path_list.size(); i++) {
                pool.EnqueueJob([&map_path_list, i]() { solt_map(map_path_list[i]); });
            }
        }
        cout << "�� ���� �Ϸ�!!!\n" << endl;

        cout << "�� �߰� ���ེ ����" << endl;
        //�� �� �߰� ���ེ
        {
            ThreadPool::ThreadPool pool(thread_size);
            for (int i = 0; i < map_path_list.size(); i++) {
                pool.EnqueueJob([&map_path_list, i]() { reduce(map_path_list[i], map_path_list[i]); });
            }
        }
        cout << "�� �߰� ���ེ �Ϸ�!!!\n" << endl;

        cout << "�ܺ����� ���� " << endl;
        //�ܺ� ����
        vector<string> map_path_final = soit_all_file(map_path_list, path_map_out, word_size);
        if (map_path_final.size() != map_path_list.size()) {
            map_path_list.clear();
        }

        //������� ���� ����
        for (size_t i = 0; i < map_path_final.size() - 1; ++i) {
            const auto& filePath = map_path_final[i];
            if (std::remove(filePath.c_str()) != 0) {
                std::perror(("Error deleting file: " + filePath).c_str());
            }
            else {
                std::cout << "File successfully deleted: " << filePath << '\n';
            }
        }
        string fianl_path = map_path_final[map_path_final.size() - 1];
        map_path_final.clear();
        cout << "�ܺ����� �Ϸ�!!! \n" << endl;

        cout << "������ ���ེ ����" << endl;
        //���ེ
        reduce(fianl_path, path_result);
    }

    cout << "Finish!!!\n" << endl;
    finish = time(NULL);
    duration = (double)(finish - start);
    minaute = duration / 60;
    sec = (int)duration % 60;

    printWordCounts();
    printf("���ð� : %d�� %d��\n", minaute, sec);
    waitForEnter();
}

//������ Ǯ�� ��� ���ϰ� mutex, lock�� ��� - ����.... ���ϴ� ����� ����(���ɺ�ȭ�� ����?)
void middle_reduce_thread_mode03() {
    //������ ������ map ��� ���� �����
    remove_befor_map_file(path_map_out);

    cout << "���� ���� ���� ����" << endl;
    start = time(NULL);

    //�������� ����
    vector<string> map_path_list = file_splite(path_in, path_map_out, block_size);
    

    //������ ���� ����
    vector<thread> threads;
    vector<bool> thread_working_cheak;
    mutex m;
    int available_thread_index = 0;

    int k = 0;
    int finish_work_count = 0;

    if (map_path_list.size() >= 1) {
        //�� ���� �� ȭ
        cout << "�� ���� ����" << endl;
        while (!(finish_work_count >= map_path_list.size())) {
            if (threads.size() < thread_size) { //�����尡 �Ѱ�ġ�� �ƴ� ��
                
                if (k < map_path_list.size()) { //�߰��� �۾��� ���� ��
                    threads.push_back(thread(thread_make_map, map_path_list[k], ref(thread_working_cheak), k, ref(m)));
                    m.lock();
                    thread_working_cheak.push_back(false);
                    m.unlock();
                    k++;
                }
                else { //���̻� �߰��� �۾��� ���� ��
                    for (int i = 0; i < threads.size(); i++) {
                        if (threads[i].joinable()) {
                            threads[i].join();
                            finish_work_count++;
                        }
                    }
                    
                }
            }
            else { //�����尡 �Ѱ�ġ ��ŭ �۵����� ��
                m.lock();
                available_thread_index = find_available_thread_index(thread_working_cheak);
                m.unlock();

                if (available_thread_index != 100) { //�۾��� ���� �����尡 ���� �� ��
                    if (k < map_path_list.size()) { //�߰��� �۾��� ���� ��
                        if (threads[available_thread_index].joinable()) {
                            threads[available_thread_index].join();
                            finish_work_count++;
                            
                            threads[available_thread_index] = thread(thread_make_map, map_path_list[k], ref(thread_working_cheak), available_thread_index, ref(m));
                            m.lock();
                            thread_working_cheak[available_thread_index] = false;
                            m.unlock();
                            k++;
                        }

                    }
                    else { //�߰��� �۾��� ���� ��
                        for (int i = 0; i < threads.size(); i++) {
                            if (threads[i].joinable()) {
                                threads[i].join();
                                finish_work_count++;
                            }

                        }

                    }

                }
                
            }


        }
        k = 0;
        finish_work_count = 0;
        available_thread_index = 0;
        threads.clear();
        thread_working_cheak.clear();
        cout << "�� ���� �Ϸ�!!!\n" << endl;

        cout << "�� ���� ����" << endl;
        //�� �� ����
        while (!(finish_work_count >= map_path_list.size())) {
            if (threads.size() < thread_size) { //�����尡 �Ѱ�ġ�� �ƴ� ��

                if (k < map_path_list.size()) { //�߰��� �۾��� ���� ��
                    threads.push_back(thread(thread_solt_map, map_path_list[k], ref(thread_working_cheak), k, ref(m)));
                    m.lock();
                    thread_working_cheak.push_back(false);
                    m.unlock();
                    k++;
                }
                else { //���̻� �߰��� �۾��� ���� ��
                    for (int i = 0; i < threads.size(); i++) {
                        if (threads[i].joinable()) {
                            threads[i].join();
                            finish_work_count++;
                        }
                    }

                }
            }
            else { //�����尡 �Ѱ�ġ ��ŭ �۵����� ��
                m.lock();
                available_thread_index = find_available_thread_index(thread_working_cheak);
                m.unlock();

                if (available_thread_index != 100) { //�۾��� ���� �����尡 ���� �� ��
                    if (k < map_path_list.size()) { //�߰��� �۾��� ���� ��
                        if (threads[available_thread_index].joinable()) {
                            threads[available_thread_index].join();
                            finish_work_count++;

                            threads[available_thread_index] = thread(thread_solt_map, map_path_list[k], ref(thread_working_cheak), available_thread_index, ref(m));
                            m.lock();
                            thread_working_cheak[available_thread_index] = false;
                            m.unlock();
                            k++;
                        }

                    }
                    else { //�߰��� �۾��� ���� ��
                        for (int i = 0; i < threads.size(); i++) {
                            if (threads[i].joinable()) {
                                threads[i].join();
                                finish_work_count++;
                            }

                        }

                    }

                }

            }


        }
        k = 0;
        finish_work_count = 0;
        available_thread_index = 0;
        threads.clear();
        thread_working_cheak.clear();
        cout << "�� ���� �Ϸ�!!!\n" << endl;

        cout << "�� �߰� ���ེ ����" << endl;
        //�� �� �߰� ���ེ
        while (!(finish_work_count >= map_path_list.size())) {
            if (threads.size() < thread_size) { //�����尡 �Ѱ�ġ�� �ƴ� ��

                if (k < map_path_list.size()) { //�߰��� �۾��� ���� ��
                    threads.push_back(thread(thread_reduce, map_path_list[k], map_path_list[k], ref(thread_working_cheak), k, ref(m)));
                    m.lock();
                    thread_working_cheak.push_back(false);
                    m.unlock();
                    k++;
                }
                else { //���̻� �߰��� �۾��� ���� ��
                    for (int i = 0; i < threads.size(); i++) {
                        if (threads[i].joinable()) {
                            threads[i].join();
                            finish_work_count++;
                        }
                    }

                }
            }
            else { //�����尡 �Ѱ�ġ ��ŭ �۵����� ��
                m.lock();
                available_thread_index = find_available_thread_index(thread_working_cheak);
                m.unlock();

                if (available_thread_index != 100) { //�۾��� ���� �����尡 ���� �� ��
                    if (k < map_path_list.size()) { //�߰��� �۾��� ���� ��
                        if (threads[available_thread_index].joinable()) {
                            threads[available_thread_index].join();
                            finish_work_count++;

                            threads[available_thread_index] = thread(thread_reduce, map_path_list[k], map_path_list[k], ref(thread_working_cheak), available_thread_index, ref(m));
                            m.lock();
                            thread_working_cheak[available_thread_index] = false;
                            m.unlock();
                            k++;
                        }

                    }
                    else { //�߰��� �۾��� ���� ��
                        for (int i = 0; i < threads.size(); i++) {
                            if (threads[i].joinable()) {
                                threads[i].join();
                                finish_work_count++;
                            }

                        }

                    }

                }

            }


        }
        k = 0;
        finish_work_count = 0;
        available_thread_index = 0;
        threads.clear();
        thread_working_cheak.clear();
        cout << "�� �߰� ���ེ �Ϸ�!!!\n" << endl;

        cout << "�ܺ����� ���� " << endl;
        //�ܺ� ����
        vector<string> map_path_final = soit_all_file(map_path_list, path_map_out, word_size);
        if (map_path_final.size() != map_path_list.size()) {
            map_path_list.clear();
        }

        //������� ���� ����
        for (size_t i = 0; i < map_path_final.size() - 1; ++i) {
            const auto& filePath = map_path_final[i];
            if (std::remove(filePath.c_str()) != 0) {
                std::perror(("Error deleting file: " + filePath).c_str());
            }
            else {
                std::cout << "File successfully deleted: " << filePath << '\n';
            }
        }
        string fianl_path = map_path_final[map_path_final.size() - 1];
        map_path_final.clear();
        cout << "�ܺ����� �Ϸ�!!! \n" << endl;

        cout << "������ ���ེ ����" << endl;
        //���ེ
        reduce(fianl_path, path_result);
    }

    cout << "Finish!!!\n" << endl;
    finish = time(NULL);
    duration = (double)(finish - start);
    minaute = duration / 60;
    sec = (int)duration % 60;

    printWordCounts();
    printf("���ð� : %d�� %d��\n", minaute, sec);
    waitForEnter();
}

//------------------------------------------------------------------------------------------------

void drawMainScreen() {
    std::cout << "\t\t================================================================\n";
    std::cout << "\t\t                      #     ###                           \n";
    std::cout << "\t\t# ## #                #    #                    #           \n";
    std::cout << "\t\t# ## #   ###  ###  ###    #      ### ## #  ### ###  ##  ### \n";
    std::cout << "\t\t#  ##   #  #  #   #  #    #     #  # #  #  # # #   # #  #   \n";
    std::cout << "\t\t## ##   #  # #    # ##    #     #  # # #  ## # #   ### #    \n";
    std::cout << "\t\t#  #    ###  #    ####     ###  ###  ###  #  # ##  ### #    \n";
    std::cout << "\t\t================================================================\n\n\n";
}
void drawCursor() {
    for (int i = 1; i <= 3; ++i) {
        std::cout << "\t\t\t\t\t";
        std::cout << (i == currentMenu ? "> " : "  ");
        switch (i) {
        case 1:
            std::cout << "Start\n";
            break;
        case 2:
            std::cout << "Info\n";
            break;
        case 3:
            std::cout << "Quit\n";
            break;
        }
    }
}
void showMenuOptions() {
    char userInput = _getch();
    switch (userInput) {
    case 72: // Up Arrow
        currentMenu = (currentMenu == 1) ? 3 : currentMenu - 1;
        break;
    case 80: // Down Arrow
        currentMenu = (currentMenu == 3) ? 1 : currentMenu + 1;
        break;
    case 13: // Enter key
        handleEnterKey();
        return;
    default:
        break;
    }

    showMainMenu();
}

//����Ű ��ġ ���� 
void handleEnterKey() {
    switch (currentMenu) {
    case 1:
        modeMenuScreen();
        break;
    case 2:
        showInfo();
        break;
    case 3:
        exitProgram();
        break;
    default:
        break;
    }
}
//�޴�ȭ�� ����ϱ�
void modeMenuScreen() {
    system("cls");
    std::cout << "\t\t\t\t***** ���� ��� ���� *****\n";
    std::cout << "\t\t\t\t 1. �⺻ ���\n";
    std::cout << "\t\t\t\t 2. �߰� ���ེ ���\n";
    std::cout << "\t\t\t\t 3. ������ �߰� ���ེ ���\n";
    std::cout << "\t\t\t\t��ȣ�� �Է��� ����� Ȯ���ϼ���\n";

    char userInput = _getch();
    int selectedDan = userInput - '0';

    if (selectedDan >= 1 && selectedDan <= 3) {
        tryProgram(selectedDan);
    }
    else {
        std::cout << "\t\t\t\t�߸��� �Է��Դϴ�.\n";
        waitForEnter();
    }
}

void tryProgram(int dan) {
    system("cls");
    std::cout << "\t\t\t\t***** " << dan << "�� �Է� ���۽����մϴ� *****\n";

    //�߰����ΰ� ��ɱ����ϱ�
    switch (dan) {
    case 1: {
        normal_mode();
        break;
    }
    case 2: {
        middle_reduce_mode();
        break;
    }
    case 3: {
        middle_reduce_thread_mode();
        //middle_reduce_thread_mode02();
        //middle_reduce_thread_mode03();
        break;
    }
    default:
        std::cout << "\t\t\t\t�߸��� �Է��Դϴ�.\n";
        waitForEnter();
        return;
    }


}

//���α׷� ����â
void showInfo() {
    system("cls");
    std::cout << "\t\t\t\t***** ���α׷� ���� *****\n";
    std::cout << "\t\t\t\t������ ���̽� �ý��� 1�й� 9��\n";
    std::cout << "\t\t\t\t�� �̸�: �°���� ������ ���Ŵ�.\n";
    std::cout << "\t\t\t\t����: ���°�,�ں���,�����,��ȣ��,\n";
    std::cout << "\t\t\t\t���� ��¥: 2023-2�б� �����ͺ��̽�\n";
    std::cout << "\t\t\t\t***********************\n";
    waitForEnter();
}

//���α׷� ����â
void exitProgram() {
    system("cls");
    std::cout << "���α׷��� �����մϴ�.\n";
    exit(0);
}

void waitForEnter() {
    std::cout << "Press Enter to continue...";
    _getch(); // Enter Ű �Է� ���
    showMainMenu();
}

void handleUserInput() {
    char userInput = _getch(); // Ű �Է� �ޱ�
    switch (userInput) {
    case '1':
        modeMenuScreen();
        break;
    case '2':
        showInfo();
        break;
    case '3':
        exitProgram();
        break;
    default:
        showMainMenu();

        break;
    }
}

void showMainMenu() {
    system("cls");
    drawMainScreen();
    drawCursor();
    showMenuOptions();
}


void wordCount() {
    cout << "ī���ó�" << endl;

    printWordCounts();
    waitForEnter();
}

void printWordCounts() {
    // ���� ����
    int counting = 0;

    std::ifstream inputFile(path_result);

    // ������ ���������� ���ȴ��� Ȯ��
    if (!inputFile.is_open()) {
        std::cerr << "Error: Failed to open the file." << std::endl;
        return;
    }
    else {
        std::cout << "\t\t\t\t***** ��� ��� *****\n";
    }

    // �� �پ� �о ���
    std::string line;
    while (std::getline(inputFile, line)) {
        std::cout << "\t\t\t" << line << std::endl;
        counting++;
    }
    std::cout << "����Ű ����: " << counting << std::endl;
    // ���� �ݱ�
    inputFile.close();
}


int main() {
    showMainMenu();
}