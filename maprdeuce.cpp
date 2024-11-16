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
#include <time.h> //시간
#include <thread> //아래쪽은 스래드 사용
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

int currentMenu = 1; // 현재 선택된 메뉴
//input 파일, map_file 폴더, result 폴더 만들고 경로 수정하기
string path_in = "./input_file/input.txt"; //원시파일 경로
string path_map_out = "./map_file/"; //map, solt 파일 넣을 경로
string path_result = "./result_file/result.txt"; //최종 파일 넣을 경로

int block_size = 100000; //파일나누는 기준 (한 파일의 최대 줄 수)
int word_size = block_size * 10;
int thread_size = 2;

double duration = 0;
int minaute = 0;
int sec = 0;
time_t start, finish;

typedef pair<string, int> KeyValuePair;

//--------------------------------  스레드 풀  -------------------------------------------------
namespace ThreadPool {
    class ThreadPool {
    public:
        ThreadPool(size_t num_threads);
        ~ThreadPool();

        // job 을 추가한다.
        void EnqueueJob(std::function<void()> job);

    private:
        // 총 Worker 쓰레드의 개수.
        size_t num_threads_;
        // Worker 쓰레드를 보관하는 벡터.
        std::vector<std::thread> worker_threads_;
        // 할일들을 보관하는 job 큐.
        std::queue<std::function<void()>> jobs_;
        // 위의 job 큐를 위한 cv 와 m.
        std::condition_variable cv_job_q_;
        std::mutex m_job_q_;

        // 모든 쓰레드 종료
        bool stop_all;

        // Worker 쓰레드
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

            // 맨 앞의 job 을 뺀다.
            std::function<void()> job = std::move(jobs_.front());
            jobs_.pop();
            lock.unlock();

            // 해당 job 을 수행한다 :)
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
            throw std::runtime_error("ThreadPool 사용 중지됨");
        }
        {
            std::lock_guard<std::mutex> lock(m_job_q_);
            jobs_.push(std::move(job));
        }
        cv_job_q_.notify_one();
    }

}  // namespace ThreadPool

//--------------------------------  파일 분할  -------------------------------------------------
//원시파일 분할 수정본
vector<string> file_splite(string path, string out_path, int block_size) {
    string line;
    vector<string> path_buffer;

    int line_count = 0;
    int block_count = 0;
    bool finish = true;

    ifstream file_read(path);

    if (file_read.is_open()) {
        while (finish) {
            string tp = out_path + to_string(block_count) + ".txt"; //중간 파일명
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

                //한 파일 마무리
                file_wr.close();
                block_count++;
                line_count = 0;
            }
        }

    }
    else {
        cerr << "원시파일 열기 실패!!!" << path << endl;
        file_read.close();
        return path_buffer;
    }

    cout << "원시파일 분할 완료!!!\n" << endl;
    file_read.close();
    return path_buffer;
}

//----------------------------------- 맵만들기 -------------------------------------------------
//파일 읽기
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

//단어 알파벳만 남기기
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

//띄어쓰기 단위로 자르기
vector<string> split(string str, char Delimiter) {
    istringstream iss(str);
    string buffer;

    vector<string> result;

    while (getline(iss, buffer, Delimiter)) {
        result.push_back(removePunctuation(buffer));
    }

    return result;
}

//맵 만들기
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

//---------------------------------- 각 맵 정렬 -------------------------------------------------
bool comp(string s1, string s2) {
    return s1 < s2;   // 사전 순
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
//-------------------------------------- 리듀스 -------------------------------------------------
void reduce(string path_map_out, string result_path) {
    vector<KeyValuePair> output;
    string line;

    string currentWord;
    int currentCount = 0;

    ifstream map_read(path_map_out);

    if (map_read.is_open()) {
        while (getline(map_read, line)) {
            istringstream iss(line);// 자료형에 맞게 읽어 줌
            string word;
            int count;

            iss >> word >> count;

            if (word == currentWord) {
                //연속된 단어 누적 카운트
                currentCount += count;
            }
            else {
                // 연속된 단어가 끝날 때 이전 연속되었던 단어 카운트 저장
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

//----------------------------------------- 외부 정렬 -------------------------------------------
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

            //while문 초기화
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
//--------------------------------------- 이전 실행기록 지우기 ----------------------------------
void remove_befor_map_file(string path_map_out) {
    string befor_path_all = path_map_out + "*.*";
    struct _finddata_t fd;
    intptr_t handle;

    vector<string> file_list; //파일 목록

    if ((handle = _findfirst(befor_path_all.c_str(), &fd)) == -1L)
        cout << "No file in directory!" << endl;

    while (_findnext(handle, &fd) == 0) {
        string tn_p = path_map_out + fd.name;
        file_list.push_back(tn_p); //파일 목록 저장
    }
    _findclose(handle);

    for (size_t i = 1; i < file_list.size(); ++i) { //첫 순서는 파일이 아님
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

//---------------------------------------- 스레드용 함수 -----------------------------------------
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
            istringstream iss(line);// 자료형에 맞게 읽어 줌
            string word;
            int count;

            iss >> word >> count;

            if (word == currentWord) {
                //연속된 단어 누적 카운트
                currentCount += count;
            }
            else {
                // 연속된 단어가 끝날 때 이전 연속되었던 단어 카운트 저장
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

//테스트 용
void thread_reduce02(string path_map_out, string result_path, vector<bool>& thread_working_cheak, int thread_index, mutex& m) {
    string currentWord;
    int currentCount = 0;

    vector<string> text_line = readFile(path_map_out);
    if (!text_line.empty()) {
        ofstream file_wr(result_path);
        if (file_wr.is_open()) {
            for (int i = 0; i < text_line.size(); i++) {
                istringstream iss(text_line[i]);// 자료형에 맞게 읽어 줌
                string word;
                int count;

                iss >> word >> count;
                

                if (word == currentWord) {
                    //연속된 단어 누적 카운트
                    currentCount += count;
                }
                else {
                    // 연속된 단어가 끝날 때 이전 연속되었던 단어 카운트 저장
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

//------------------------------------------ 버전 나누기 -----------------------------------------

void normal_mode() {
    //이전에 생성한 map 결과 파일 지우기
    remove_befor_map_file(path_map_out);
    start = time(NULL);
    //원시파일 분할
    cout << "원시파일 분할 시작" << endl;
    vector<string> map_path_list = file_splite(path_in, path_map_out, block_size);

    if (map_path_list.size() >= 1) {
        //각 분할파일 맵 화 시키기
        cout << "맵 연산 시작" << endl;
        for (int i = 0; i < map_path_list.size(); i++) {
            make_map(map_path_list[i]);
        }
        cout << "맵 연산 완료!!!\n" << endl;

        //각 맵 정렬
        cout << "맵 정렬 시작" << endl;
        for (int i = 0; i < map_path_list.size(); i++) {
            solt_map(map_path_list[i]);
        }
        cout << "맵 정렬 완료!!!\n" << endl;

        //외부 정렬
        cout << "외부정렬 시작" << endl;
        vector<string> map_path_final = soit_all_file(map_path_list, path_map_out, word_size);
        if (map_path_final.size() != map_path_list.size()) {
            map_path_list.clear();
        }

        //쓸모없는 파일 제거
        for (size_t i = 0; i < map_path_final.size() - 1; ++i) {
            const auto& filePath = map_path_final[i];
            if (std::remove(filePath.c_str()) != 0) {
                std::perror(("Error deleting file: " + filePath).c_str());
            }
            else {
                std::cout << "File successfully deleted: " << filePath << '\n';
            }
        }
        cout << "외부정렬 완료!!!\n" << endl;
        string fianl_path = map_path_final[map_path_final.size() - 1];
        map_path_final.clear();

        //리듀스
        cout << "마지막 리듀스 시작" << endl;
        reduce(fianl_path, path_result);
    }

    cout << "Finsh!!!\n" << endl;
    finish = time(NULL);
    duration = (double)(finish - start);
    minaute = duration / 60;
    sec = (int)duration % 60;

    printWordCounts();
    printf("사용시간 : %d분 %d초\n", minaute, sec);
    waitForEnter();
}

void middle_reduce_mode() {
    //이전에 생성한 map 결과 파일 지우기
    remove_befor_map_file(path_map_out);
    start = time(NULL);
    //원시파일 분할
    cout << "원시파일 분할 시작" << endl;
    vector<string> map_path_list = file_splite(path_in, path_map_out, block_size);

    if (map_path_list.size() >= 1) {
        //각 분할파일 맵 화 시키기
        cout << "맵 연산 시작" << endl;
        for (int i = 0; i < map_path_list.size(); i++) {
            make_map(map_path_list[i]);
        }
        cout << "맵 연산 완료!!!\n" << endl;

        //각 맵 정렬
        cout << "맵 정렬 시작" << endl;
        for (int i = 0; i < map_path_list.size(); i++) {
            solt_map(map_path_list[i]);
        }
        cout << "맵 정렬 완료!!!\n" << endl;

        //중간 리듀스
        cout << "맵 중간 리듀스 시작" << endl;
        for (int i = 0; i < map_path_list.size(); i++) {
            reduce(map_path_list[i], map_path_list[i]);
        }
        cout << "맵 중간 리듀스 완료!!!\n" << endl;

        //외부 정렬
        cout << "외부정렬 시작" << endl;
        vector<string> map_path_final = soit_all_file(map_path_list, path_map_out, word_size);
        if (map_path_final.size() != map_path_list.size()) {
            map_path_list.clear();
        }

        //쓸모없는 파일 제거
        for (size_t i = 0; i < map_path_final.size() - 1; ++i) {
            const auto& filePath = map_path_final[i];
            if (std::remove(filePath.c_str()) != 0) {
                std::perror(("Error deleting file: " + filePath).c_str());
            }
            else {
                std::cout << "File successfully deleted: " << filePath << '\n';
            }
        }
        cout << "외부정렬 완료!!!\n" << endl;
        string fianl_path = map_path_final[map_path_final.size() - 1];
        map_path_final.clear();

        //리듀스
        cout << "마지막 리듀스 시작" << endl;
        reduce(fianl_path, path_result);
    }

    cout << "Finsh!!!\n" << endl;
    finish = time(NULL);
    duration = (double)(finish - start);
    minaute = duration / 60;
    sec = (int)duration % 60;

    printWordCounts();
    printf("사용시간 : %d분 %d초\n", minaute, sec);
    waitForEnter();
}

//확률의존 스레드 사용
void middle_reduce_thread_mode() {
    //이전에 생성한 map 결과 파일 지우기
    remove_befor_map_file(path_map_out);

    cout << "원시 파일 분할 시작" << endl;
    start = time(NULL);

    //원시파일 분할
    vector<string> map_path_list = file_splite(path_in, path_map_out, block_size);

    //스레드 관리 벡터
    vector<thread> threads;
    int k = 0;
    int finish_work_count = 0;
    
    if (map_path_list.size() >= 1) {
        //각 파일 맵 화
        cout << "맵 연산 시작" << endl;
        while (!(finish_work_count == map_path_list.size())) {
            if (threads.size() < thread_size) { //스레드가 한계치가 아닐 때
                if (k < map_path_list.size()) {
                    threads.push_back(thread(make_map, map_path_list[k]));
                    k++;
                }
                else { //더이상 추가할 작업이 없을 때
                    if (threads[0].joinable()) {
                        threads[0].join();
                        if (threads.size() != 1)
                            threads.erase(threads.begin());
                        finish_work_count++;
                    }
                }
            }
            else { //스레드가 한계치 많큼 작동중일 때
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
        cout << "맵 연산 완료!!!\n" << endl;

        cout << "맵 정렬 시작" << endl;
        //각 맵 정렬
        while (!(finish_work_count == map_path_list.size())) {
            if (threads.size() < thread_size) {
                if (k < map_path_list.size()) {
                    threads.push_back(thread(solt_map, map_path_list[k]));
                    k++;
                }
                else { //더이상 추가할 작업이 없을 때
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
        cout << "맵 정렬 완료!!!\n" << endl;

        cout << "맵 중간 리듀스 시작" << endl;
        //각 맵 중간 리듀스
        while (!(finish_work_count == map_path_list.size())) {
            if (threads.size() < thread_size) {
                if (k < map_path_list.size()) {
                    threads.push_back(thread(reduce, map_path_list[k], map_path_list[k]));
                    k++;
                }
                else { //더이상 추가할 작업이 없을 때
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
        cout << "맵 중간 리듀스 완료!!!\n" << endl;

        cout << "외부정렬 시작 " << endl;
        //외부 정렬
        vector<string> map_path_final = soit_all_file(map_path_list, path_map_out, word_size);
        if (map_path_final.size() != map_path_list.size()) {
            map_path_list.clear();
        }

        //쓸모없는 파일 제거
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
        cout << "외부정렬 완료!!! \n" << endl;

        cout << "마지막 리듀스 시작" << endl;
        //리듀스
        reduce(fianl_path, path_result);
    }

    cout << "Finish!!!\n" << endl;
    finish = time(NULL);
    duration = (double)(finish - start);
    minaute = duration / 60;
    sec = (int)duration % 60;

    printWordCounts();
    printf("사용시간 : %d분 %d초\n", minaute, sec);
    waitForEnter();
}

//스레드 풀 사용
void middle_reduce_thread_mode02() {
    //이전에 생성한 map 결과 파일 지우기
    remove_befor_map_file(path_map_out);

    cout << "원시 파일 분할 시작" << endl;
    start = time(NULL);

    //원시파일 분할
    vector<string> map_path_list = file_splite(path_in, path_map_out, block_size);

    if (map_path_list.size() >= 1) {
        //각 파일 맵 화
        cout << "맵 연산 시작" << endl;
        {
            ThreadPool::ThreadPool pool(thread_size);
            for (int i = 0; i < map_path_list.size(); i++) {
                pool.EnqueueJob([&map_path_list, i]() { make_map(map_path_list[i]); });
            }
        }
        cout << "맵 연산 완료!!!\n" << endl;

        cout << "맵 정렬 시작" << endl;
        //각 맵 정렬
        {
            ThreadPool::ThreadPool pool(thread_size);
            for (int i = 0; i < map_path_list.size(); i++) {
                pool.EnqueueJob([&map_path_list, i]() { solt_map(map_path_list[i]); });
            }
        }
        cout << "맵 정렬 완료!!!\n" << endl;

        cout << "맵 중간 리듀스 시작" << endl;
        //각 맵 중간 리듀스
        {
            ThreadPool::ThreadPool pool(thread_size);
            for (int i = 0; i < map_path_list.size(); i++) {
                pool.EnqueueJob([&map_path_list, i]() { reduce(map_path_list[i], map_path_list[i]); });
            }
        }
        cout << "맵 중간 리듀스 완료!!!\n" << endl;

        cout << "외부정렬 시작 " << endl;
        //외부 정렬
        vector<string> map_path_final = soit_all_file(map_path_list, path_map_out, word_size);
        if (map_path_final.size() != map_path_list.size()) {
            map_path_list.clear();
        }

        //쓸모없는 파일 제거
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
        cout << "외부정렬 완료!!! \n" << endl;

        cout << "마지막 리듀스 시작" << endl;
        //리듀스
        reduce(fianl_path, path_result);
    }

    cout << "Finish!!!\n" << endl;
    finish = time(NULL);
    duration = (double)(finish - start);
    minaute = duration / 60;
    sec = (int)duration % 60;

    printWordCounts();
    printf("사용시간 : %d분 %d초\n", minaute, sec);
    waitForEnter();
}

//스레드 풀은 사용 안하고 mutex, lock만 사용 - 최종.... 원하는 모양의 동작(성능변화는 없음?)
void middle_reduce_thread_mode03() {
    //이전에 생성한 map 결과 파일 지우기
    remove_befor_map_file(path_map_out);

    cout << "원시 파일 분할 시작" << endl;
    start = time(NULL);

    //원시파일 분할
    vector<string> map_path_list = file_splite(path_in, path_map_out, block_size);
    

    //스레드 관리 벡터
    vector<thread> threads;
    vector<bool> thread_working_cheak;
    mutex m;
    int available_thread_index = 0;

    int k = 0;
    int finish_work_count = 0;

    if (map_path_list.size() >= 1) {
        //각 파일 맵 화
        cout << "맵 연산 시작" << endl;
        while (!(finish_work_count >= map_path_list.size())) {
            if (threads.size() < thread_size) { //스레드가 한계치가 아닐 때
                
                if (k < map_path_list.size()) { //추가할 작없이 있을 때
                    threads.push_back(thread(thread_make_map, map_path_list[k], ref(thread_working_cheak), k, ref(m)));
                    m.lock();
                    thread_working_cheak.push_back(false);
                    m.unlock();
                    k++;
                }
                else { //더이상 추가할 작업이 없을 때
                    for (int i = 0; i < threads.size(); i++) {
                        if (threads[i].joinable()) {
                            threads[i].join();
                            finish_work_count++;
                        }
                    }
                    
                }
            }
            else { //스레드가 한계치 많큼 작동중일 때
                m.lock();
                available_thread_index = find_available_thread_index(thread_working_cheak);
                m.unlock();

                if (available_thread_index != 100) { //작업이 끝난 스레드가 존재 할 때
                    if (k < map_path_list.size()) { //추가할 작없이 있을 때
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
                    else { //추가할 작업이 없을 때
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
        cout << "맵 연산 완료!!!\n" << endl;

        cout << "맵 정렬 시작" << endl;
        //각 맵 정렬
        while (!(finish_work_count >= map_path_list.size())) {
            if (threads.size() < thread_size) { //스레드가 한계치가 아닐 때

                if (k < map_path_list.size()) { //추가할 작없이 있을 때
                    threads.push_back(thread(thread_solt_map, map_path_list[k], ref(thread_working_cheak), k, ref(m)));
                    m.lock();
                    thread_working_cheak.push_back(false);
                    m.unlock();
                    k++;
                }
                else { //더이상 추가할 작업이 없을 때
                    for (int i = 0; i < threads.size(); i++) {
                        if (threads[i].joinable()) {
                            threads[i].join();
                            finish_work_count++;
                        }
                    }

                }
            }
            else { //스레드가 한계치 많큼 작동중일 때
                m.lock();
                available_thread_index = find_available_thread_index(thread_working_cheak);
                m.unlock();

                if (available_thread_index != 100) { //작업이 끝난 스레드가 존재 할 때
                    if (k < map_path_list.size()) { //추가할 작없이 있을 때
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
                    else { //추가할 작업이 없을 때
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
        cout << "맵 정렬 완료!!!\n" << endl;

        cout << "맵 중간 리듀스 시작" << endl;
        //각 맵 중간 리듀스
        while (!(finish_work_count >= map_path_list.size())) {
            if (threads.size() < thread_size) { //스레드가 한계치가 아닐 때

                if (k < map_path_list.size()) { //추가할 작없이 있을 때
                    threads.push_back(thread(thread_reduce, map_path_list[k], map_path_list[k], ref(thread_working_cheak), k, ref(m)));
                    m.lock();
                    thread_working_cheak.push_back(false);
                    m.unlock();
                    k++;
                }
                else { //더이상 추가할 작업이 없을 때
                    for (int i = 0; i < threads.size(); i++) {
                        if (threads[i].joinable()) {
                            threads[i].join();
                            finish_work_count++;
                        }
                    }

                }
            }
            else { //스레드가 한계치 많큼 작동중일 때
                m.lock();
                available_thread_index = find_available_thread_index(thread_working_cheak);
                m.unlock();

                if (available_thread_index != 100) { //작업이 끝난 스레드가 존재 할 때
                    if (k < map_path_list.size()) { //추가할 작없이 있을 때
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
                    else { //추가할 작업이 없을 때
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
        cout << "맵 중간 리듀스 완료!!!\n" << endl;

        cout << "외부정렬 시작 " << endl;
        //외부 정렬
        vector<string> map_path_final = soit_all_file(map_path_list, path_map_out, word_size);
        if (map_path_final.size() != map_path_list.size()) {
            map_path_list.clear();
        }

        //쓸모없는 파일 제거
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
        cout << "외부정렬 완료!!! \n" << endl;

        cout << "마지막 리듀스 시작" << endl;
        //리듀스
        reduce(fianl_path, path_result);
    }

    cout << "Finish!!!\n" << endl;
    finish = time(NULL);
    duration = (double)(finish - start);
    minaute = duration / 60;
    sec = (int)duration % 60;

    printWordCounts();
    printf("사용시간 : %d분 %d초\n", minaute, sec);
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

//엔터키 위치 제어 
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
//메뉴화면 출력하기
void modeMenuScreen() {
    system("cls");
    std::cout << "\t\t\t\t***** 연산 모드 선택 *****\n";
    std::cout << "\t\t\t\t 1. 기본 모드\n";
    std::cout << "\t\t\t\t 2. 중간 리듀스 모드\n";
    std::cout << "\t\t\t\t 3. 스레드 중간 리듀스 모드\n";
    std::cout << "\t\t\t\t번호를 입력해 결과를 확인하세요\n";

    char userInput = _getch();
    int selectedDan = userInput - '0';

    if (selectedDan >= 1 && selectedDan <= 3) {
        tryProgram(selectedDan);
    }
    else {
        std::cout << "\t\t\t\t잘못된 입력입니다.\n";
        waitForEnter();
    }
}

void tryProgram(int dan) {
    system("cls");
    std::cout << "\t\t\t\t***** " << dan << "번 입력 동작시작합니다 *****\n";

    //추가적인거 기능구현하기
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
        std::cout << "\t\t\t\t잘못된 입력입니다.\n";
        waitForEnter();
        return;
    }


}

//프로그램 정보창
void showInfo() {
    system("cls");
    std::cout << "\t\t\t\t***** 프로그램 정보 *****\n";
    std::cout << "\t\t\t\t데이터 베이스 시스템 1분반 9조\n";
    std::cout << "\t\t\t\t팀 이름: 태경님이 축지법 쓰신다.\n";
    std::cout << "\t\t\t\t팀원: 김태경,박병욱,서기원,이호준,\n";
    std::cout << "\t\t\t\t제작 날짜: 2023-2학기 데이터베이스\n";
    std::cout << "\t\t\t\t***********************\n";
    waitForEnter();
}

//프로그램 종료창
void exitProgram() {
    system("cls");
    std::cout << "프로그램을 종료합니다.\n";
    exit(0);
}

void waitForEnter() {
    std::cout << "Press Enter to continue...";
    _getch(); // Enter 키 입력 대기
    showMainMenu();
}

void handleUserInput() {
    char userInput = _getch(); // 키 입력 받기
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
    cout << "카운팅끝" << endl;

    printWordCounts();
    waitForEnter();
}

void printWordCounts() {
    // 파일 열기
    int counting = 0;

    std::ifstream inputFile(path_result);

    // 파일이 성공적으로 열렸는지 확인
    if (!inputFile.is_open()) {
        std::cerr << "Error: Failed to open the file." << std::endl;
        return;
    }
    else {
        std::cout << "\t\t\t\t***** 결과 출력 *****\n";
    }

    // 한 줄씩 읽어서 출력
    std::string line;
    while (std::getline(inputFile, line)) {
        std::cout << "\t\t\t" << line << std::endl;
        counting++;
    }
    std::cout << "고유키 개수: " << counting << std::endl;
    // 파일 닫기
    inputFile.close();
}


int main() {
    showMainMenu();
}