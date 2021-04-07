#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUF_SIZE 1000
#define MAX_CLIENTS 5

#define MAX_NAME_SIZE 500
#define MAX_PASS_SIZE 500

#define EMPTY 0
#define CONNECTED 1
#define LOGIN 2

#define NotPlaying 0
#define Playing 1

#define DELETED_INDEX -1

void addClient(int s, struct sockaddr_in *newclnt_adr);
void removeClient(int s);
void error_handling(char *msg);
int who_win(int a, int b);

int result;
double gameCounts = 0;
int gameMember = 0;
double win_cnt1 = 0, win_cnt2 = 0;
char win[] = "You win\n", lose[] = "You lose\n", draw[] = "DRAW\n", end[] = "Game finish\n";

int serv_sock, clnt_sock;
int clnt_cnt = 0;

fd_set reads, cpy_reads;
int fd_max = 0;

typedef struct userInfo {
	int gameFlag;
	int loginFlag;
	int sockNum;
	char nameArr[MAX_NAME_SIZE];
	char passArr[MAX_PASS_SIZE];
}userInfo;

typedef struct game {
	int value;
	int socket;
}game;

game player[2];
userInfo user[MAX_CLIENTS];
int banList[MAX_CLIENTS][MAX_CLIENTS];

int main(int argc, char *argv[])
{
	struct sockaddr_in clnt_adr, serv_adr;
	struct timeval timeout;

	socklen_t adr_sz;
	int fd_num, i, j, k, msg_len, option = 1;

	char *ptr;
	char buf[BUF_SIZE + 1];
	char temp_buf[BUF_SIZE + 1];

	FILE *input = NULL;

	if (argc != 2)
	{
		printf("Usage : %s <port>\n", argv[0]);
		exit(0);
	}


	serv_sock = socket(AF_INET, SOCK_STREAM, 0);

	if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&option, sizeof(option)) != 0)
		error_handling("setsocket() error");

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));


	if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind error");

	if (listen(serv_sock, 5) == -1)
		error_handling("listen error");
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		user[i].sockNum = DELETED_INDEX;
		user[i].gameFlag = NotPlaying;
		user[i].loginFlag = EMPTY;
		for (j = 0; j < MAX_CLIENTS; j++)
			banList[i][j] = 0;
	}

	FD_ZERO(&reads);
	FD_SET(serv_sock, &reads);
	fd_max = serv_sock;

	while (1)
	{
		cpy_reads = reads;
		timeout.tv_sec = 5;
		timeout.tv_usec = 5000;

		if ((fd_num = select(fd_max + 1, &cpy_reads, 0, 0, &timeout)) == -1)
			break;
		
		if (fd_num == 0)
			continue;

		if (FD_ISSET(serv_sock, &cpy_reads))
		{
			clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &adr_sz);
			addClient(clnt_sock, &clnt_adr);
		}

		for (i = 0; i<MAX_CLIENTS; i++) //연결된 클라이언트의 수 만큼 반복문
		{
			if (user[i].loginFlag == EMPTY)
				continue;
			if (FD_ISSET(user[i].sockNum, &cpy_reads)) // 디스크립터 정보가 있으면
			{
				if ((msg_len = recv(user[i].sockNum, buf, BUF_SIZE, 0)) <= 0)   // recv함수의 응답이 없으면 (음수반환) 해당 클라이언트 소켓 해제함.
				{
					removeClient(i); // 클라이언트 제거 함수 호출
					continue;
				}
				
				buf[msg_len] = 0;

				if (user[i].loginFlag == LOGIN) // 로그인이 된 경우
				{
					if (user[i].gameFlag == NotPlaying) // 게임에 진입하지 않은 상태일 때
					{
						strcpy(temp_buf, buf);
						ptr = strtok(temp_buf, " ");

						if (!strcmp(ptr, "/w")) // 귓속말
						{
							ptr = strtok(NULL, " ");

							for (j = 0; j < MAX_CLIENTS; j++)
							{
								if (!strcmp(user[j].nameArr, ptr) && user[j].loginFlag == LOGIN)
								{
									ptr = strtok(NULL, " ");
									k = 0;
									while (ptr != NULL)
									{
										strcpy(&buf[k], ptr);
										k += strlen(ptr);
										buf[k] = ' ';
										k++;
										ptr = strtok(NULL, " ");
									}
									buf[k] = 0;

									sprintf(temp_buf, "%s)%s", user[i].nameArr, buf);

									if (banList[i][j] != 1)
										send(user[j].sockNum, temp_buf, strlen(temp_buf) - 1, 0);

									break;
								}
							}
								
							if (j == MAX_CLIENTS)
							{
								send(user[i].sockNum, "Whisper fail, no user\n", strlen("Whisper fail, no user\n"), 0);
							}
						}
						else if (!strcmp(ptr, "/b")) // 차단
						{
							if (user[i].loginFlag == LOGIN)
							{
								ptr = strtok(NULL, " ");
								ptr[strlen(ptr) - 1] = 0;

								for (j = 0; j < MAX_CLIENTS; j++)
								{
									if (user[j].nameArr == NULL)
										break;

									if (!strcmp(user[j].nameArr, ptr))
									{
										banList[j][i] = 1;
										break;
									}
								}
							}
						}
						else if (!strcmp(ptr, "/game\n")) // 가위바위보
						{
							if (gameMember < 2)
							{
								user[i].gameFlag = Playing;
								player[gameMember++].socket = user[i].sockNum;
								if (gameMember == 2)
								{
									puts("New game start");

									for (j = 0; j < 2; j++)
									{
										send(player[j].socket, "Input your decision\n", strlen("Input your decision\n"), 0);
									}
								}
							}
							else
							{
								send(user[i].sockNum, "Another game is already playing.\n", strlen("Another game is already playing.\n"), 0);
							}
						}
						else // 일반 전체 채팅의 경우
						{
							sprintf(temp_buf, "%s)%s", user[i].nameArr, buf);
							for (j = 0; j < MAX_CLIENTS; j++)
							{
								if (user[j].loginFlag == LOGIN && i != j && banList[i][j] != 1) // 로그인이 된 클라이언트들에게 메시지 보내기
								{
									send(user[j].sockNum, temp_buf, strlen(temp_buf), 0);
								}
							}
						}
					}
					else if(user[i].gameFlag == Playing) // game에 진입한 상태일 때
					{
						if (gameMember == 2)
						{
							for (j = 0; j < 2; j++)
							{
								if (user[i].sockNum == player[j].socket)
								{
									buf[2] = 0;

									if (!strcmp(buf, "/p")) // 게임 종료 프로세스
									{
										if (gameCounts == 0)
											sprintf(buf, "no game processed.");
										else
											sprintf(buf, "%.2lf  %.2lf", (win_cnt1 / gameCounts)*100.0, (win_cnt2 / gameCounts)*100.0);

										puts(buf);// 각 승률 출력
										send(player[0].socket, "Game finish\n", strlen("Game finish\n"), 0);
										send(player[1].socket, "Game finish\n", strlen("Game finish\n"), 0);
										gameMember = 0;
										gameCounts = 0;
										win_cnt1 = win_cnt2 = 0;
										for (k = 0; k < MAX_CLIENTS; k++)
										{
											user[k].gameFlag = NotPlaying;
										}
										break;
									}
									else // 뭐 냈는지 기록
									{
										buf[1] = 0;
										player[j].value = atoi(buf);
										if (player[j].value != 1 && player[j].value != 2 && player[j].value != 3)
										{
											player[j].value = 0;
											send(player[j].socket, "Incorrect input, try again\n", strlen("Incorrect input, try again\n"), 0);
											break;
										}
									}
								}
							}

							if (player[0].value != 0 && player[1].value != 0)
							{
								result = who_win(player[0].value, player[1].value);
								if (result == 0)
								{
									write(player[0].socket, draw, sizeof(draw));
									write(player[1].socket, draw, sizeof(draw));
								}
								else if (result == 1)
								{
									win_cnt1++;
									write(player[0].socket, win, sizeof(win));
									write(player[1].socket, lose, sizeof(lose));
								}
								else
								{
									win_cnt2++;
									write(player[0].socket, lose, sizeof(lose));
									write(player[1].socket, win, sizeof(win));
								}

								gameCounts++;
								player[0].value = 0;
								player[1].value = 0;
							}
						}
					}
				}
				else if (user[i].loginFlag == CONNECTED) // 로그인 되지 않은 경우
				{
					strcpy(temp_buf, buf);
					ptr = strtok(temp_buf, " ");
					if (!strcmp(ptr, "/register")) // 회원가입을 요청한 경우
					{
						ptr = strtok(NULL, " ");

						sprintf(buf, "%s.txt", ptr);
						if ((input = fopen(buf, "r")) != NULL)
						{
							send(user[i].sockNum, "Incorrect registration\n", strlen("Incorrect registration\n"), 0);
							puts("Incorrect registration");
							fclose(input);
							break;
						}

						strcpy(user[i].nameArr, ptr);
						ptr = strtok(NULL, " ");
						strcpy(user[i].passArr, ptr);

						sprintf(buf, "%s is registered", user[i].nameArr);
						puts(buf);
						sprintf(buf, "Registration success\n");
						send(user[i].sockNum, buf, strlen(buf), 0);

						sprintf(buf, "%s.txt", user[i].nameArr);
						input = fopen(buf, "w");
						fprintf(input, "%s", user[i].passArr);
						fclose(input);

						memset(user[i].nameArr, 0, sizeof(user[i].nameArr));
						memset(user[i].passArr, 0, sizeof(user[i].nameArr));

					}
					else if (!strcmp(ptr, "/login")) // 로그인을 요청한 경우
					{
						ptr = strtok(NULL, " ");

						for (j = 0; j < MAX_CLIENTS; j++)
						{
							if (!strcmp(user[j].nameArr, ptr)) {
								send(user[i].sockNum, "Login fail\n", strlen("Login fail\n"), 0);
								break;
							}
						}

						if (j == MAX_CLIENTS)
						{
							strcpy(user[i].nameArr, ptr);
							sprintf(buf, "%s.txt", ptr);

							if ((input = fopen(buf, "r")) != NULL)
							{
								ptr = strtok(NULL, " ");
								fscanf(input, "%s", user[i].passArr);

								ptr[strlen(ptr) - 1] = 0;

								if (!strcmp(user[i].passArr, ptr))
								{
									send(user[i].sockNum, "Login successful\n", strlen("Login successful\n"), 0);
									user[i].loginFlag = LOGIN;
								}
							}
							else
								send(user[i].sockNum, "Login fail\n", strlen("Login fail\n"), 0);
						}
					}
					else // 회원가입요청 또는 로그인 요청이 아닌 경우엔 Echo
					{
						send(user[i].sockNum, buf, strlen(buf), 0);
						break;
					}
				}
			}
		}
	}
	return 0;
}

void addClient(int s, struct sockaddr_in *newclnt_adr)
{
	char buf[40];
	char curStat[50];
	int i;

	if (clnt_cnt == 5)
	{
		send(s, "We can afford clients up to 5.\n", strlen("We can afford clients up to 5.\n"), 0);
		close(s);
		return;
	}

	if (fd_max < s)
		fd_max = s;

	FD_SET(s, &reads);
	for (i = 0; i < MAX_CLIENTS; i++)
		if (user[i].sockNum == DELETED_INDEX)
		{
			user[i].loginFlag = CONNECTED;
			user[i].sockNum = s;
			clnt_cnt++;
			break;
		}
	
	sprintf(buf, "Connection Success, Your ID is User%d\n", i + 1);
	send(user[i].sockNum, buf, strlen(buf), 0);

	sprintf(curStat, "New clients is connected - Number of total clients is %d", clnt_cnt);
	puts(curStat);
}

void removeClient(int i)
{
	int count;
	char curStat[50];

	FD_CLR(user[i].sockNum, &reads);
	close(user[i].sockNum);
	memset(user[i].nameArr, 0, sizeof(user[i].nameArr));
	user[i].sockNum = DELETED_INDEX;
	user[i].loginFlag = EMPTY;

	fd_max = serv_sock;
	for (count = 0; count < MAX_CLIENTS; count++)
		if (user[count].sockNum > fd_max && user[count].loginFlag != EMPTY)
			fd_max = user[count].sockNum;
	
	for (count = 0; count < MAX_CLIENTS; count++)
	{
		banList[count][i] = 0;
		banList[i][count] = 0;
	}

	sprintf(curStat, "User %d is disconnected - Number of total clients is %d", i + 1, --clnt_cnt);
	puts(curStat);
}

void error_handling(char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}

int who_win(int a, int b)
{
	if (a == b)
		return 0;
	else if (a % 3 == (b + 1) % 3)
		return 1;
	else
		return -1;
}