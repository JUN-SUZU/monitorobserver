#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <csignal>
#include <sys/wait.h>
#include <vector>
#include <string>
using namespace std;

void sigchld_handler(int signo)
{
    // 子プロセスの終了を待機してゾンビプロセス化を防ぐ
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

bool isAlive(pid_t pid)
{
    return kill(pid, 0) == 0;
    // return waitpid(pid, nullptr, WNOHANG) == 0;
}

vector<vector<string>> readConfig()
{
    // サーバーの設定ファイルを読み込む
    // 書式
    // 1行に1つのサーバーの設定
    // 要素はスペース区切り 波かっこ2つで囲む例: {{}} {{}}
    // 空行を間に挟むとエラーになる
    // 最終行に空行を入れても構わない
    // 1列目: サーバーの名前
    // 2列目: サーバーのルートディレクトリの絶対パス
    // 3列目: サーバーの起動コマンド(コマンドは/bin不要, ファイル名はフルパス)
    // 4列目: 設定したいサーバーの状態(0: offline, 1: online)(0でオンラインになっていたら停止する, 1でオフラインになっていたら起動する)
    // 例
    // {{server1}} {{/var/www/html}} {{node /var/www/html/index.js}} {{0}}
    // {{server2}} {{/home/user/projects/app}} {{node /home/jun/projects/app/index.js}} {{1}}
    vector<vector<string>> servers;
    ifstream conf("servers.conf");
    if (!conf)
    {
        cerr << "Failed to open servers.conf" << endl;
        exit(1);
    }
    string line;
    while (getline(conf, line))
    {
        if (line.empty())// 空行なら以降の行は読み込まない
        {
            break;
        }
        vector<string> server;
        string element;
        for (int i = 0; i < 4; i++)
        {
            size_t start = line.find("{{");
            size_t end = line.find("}}");
            element = line.substr(start + 2, end - start - 2);
            line = line.substr(end + 2);
            server.push_back(element);
        }
        servers.push_back(server);
    }
    conf.close(); // ファイルを閉じる
    return servers;
}

int main()
{
    vector<vector<string>> servers;
    servers = readConfig();
    int P_MAX = servers.size();
    int status[P_MAX];
    int pid[P_MAX];
    signal(SIGCHLD, sigchld_handler);

    int i;
    for (i = 0; i < P_MAX && (pid[i] = fork()) > 0; i++);
    cout << "Process started with PID: " << pid[i] << endl;

    if (i == P_MAX)
    { // 親プロセスはすべての子プロセスの終了を待つ
        while (true)
        {
            servers = readConfig();
            P_MAX = servers.size();
            for (int j = 0; j < P_MAX; j++)
            {
                if (isAlive(pid[j]))
                {
                    cout << "Process " << j << " is still running..." << servers[j][0] << endl;
                    if (servers[j][3] == "0")
                    {
                        // サーバーを停止する
                        kill(pid[j], SIGKILL);
                    }
                }
                else
                {
                    cout << "Process " << j << " has terminated." << servers[j][0] << endl;
                    if (servers[j][3] == "1")
                    {
                        // サーバーを再起動する
                        // 新しいプロセスを生成して置き換える
                        pid[j] = fork();
                        if (pid[j] == 0)
                        { // 子プロセス
                            cout << "child:" << j << endl;
                            if (servers[j][3] == "1")
                            {
                                // サーバーを起動する
                                string cmd = servers[j][2].substr(0, servers[j][2].find(" "));
                                string arg = servers[j][2].substr(servers[j][2].find(" ") + 1);
                                execlp(cmd.c_str(), cmd.c_str(), arg.c_str(), (char *)nullptr);
                            }
                            exit(0);
                            _exit(127);
                        }
                    }
                }
            }
            sleep(5);
        }
    }
    else if (pid[i] == 0)
    { // 子プロセス
        printf("child:%d\n", i);
        if (servers[i][3] == "1")
        {
            // サーバーを起動する
            string cmd = servers[i][2].substr(0, servers[i][2].find(" "));
            string arg = servers[i][2].substr(servers[i][2].find(" ") + 1);
            execlp(cmd.c_str(), cmd.c_str(), arg.c_str(), (char *)nullptr);
        }
        exit(0);
        _exit(127);
    }
    else
    {
        perror("child process");
        exit(0);
    }
    return 0;
}
