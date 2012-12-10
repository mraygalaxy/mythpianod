/*
Copyright (c) 2010
	Doug Turner < dougt@dougt.org >

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef MYTHPANDORA_H
#define MYTHPANDORA_H

// MythTV headers
#include <QTimer>
#include <QHttp>
#include <QTemporaryFile>


#include "mythscreentype.h"
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuiimage.h"
#include "mythuitextedit.h"
#include "audiooutput.h"
#include <pthread.h>

extern "C" {
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
}

class MythPianoService;
MythPianoService * GetMythPianoService();

int showPopupDialog();
int showLoginDialog();
int showStationSelectDialog();
int showPlayerDialog();

class MythPianoResponse { 
  public: 
	int code; 
	string value; 
        MythPianoResponse(int c, string v) 
	{
		code = c;
		value = v;
	}
};

class MythPianoServiceListener
{
 public:
  virtual void RecvMessage(const char* message) = 0;
};

class MythPianoService : public QObject
{
 Q_OBJECT

 public:
  MythPianoService();
  ~MythPianoService();

  int  Login();
  void Logout();
  void PauseToggle();
  int GetPlaylist();

  void StartPlayback();
  void StartPlayerThread();
  void StopPlayerThread();
  void StopPlayback();
  void NextSong();

  void VolumeUp();
  void VolumeDown();
  int  Volume();
  void ToggleMute();

  void BroadcastMessage(const char *format, ...);

  void SetMessageListener(MythPianoServiceListener* listener);
  void RemoveMessageListener(MythPianoServiceListener* listener);

  map<string, string> GetCurrentSong() { return current_song; };
  int SongChanged() { if(song_changed) { song_changed = 0; return 1; } return 0;};
  void SkipSong() { rlen = sprintf(request, "skip\n"); SendPianodRequest(200); }
  void TiredSong() { rlen = sprintf(request, "rate overplayed\n"); SendPianodRequest(200); }
  void HateSong() { rlen = sprintf(request, "rate bad\n"); SendPianodRequest(200); }
  void LoveSong() { rlen = sprintf(request, "rate good\n"); SendPianodRequest(200); }
  void UnloveSong() { rlen = sprintf(request, "rate neutral\n"); SendPianodRequest(200); }
  vector<string> GetStations() { return stations; };
  string GetCurrentStation() { if(current_station != -1) return stations[current_station]; else return ""; };
  void GetTimes(string *played, string *duration);
  string	     current_station_name;
  int 		     current_station;
  void SetCurrentStation(QString name);

 private:
  void CheckForResponse(int success1, int success2, int success3, int success4, int len);
  map<string, string> PullOutSong(int idx);
  std::vector<MythPianoResponse> *GetPianodLines(int success1, int success2, int success3, int success4);
  int SendPianodRequest(int success);
  void PianodDisconnect(std::string msg);
  int RepopulateStations();

  pthread_t          m_PlayerThread;

  int song_changed;
  string duration;
  string played;
  map<string, string> current_song;
  vector<map<string,string> > playlist;
  vector<string>     stations;

  MythPianoServiceListener* m_Listener;

  QTimer*            m_Timer;
  vector<MythPianoResponse> *response;
  
  struct sockaddr_in pianod_addr;
  char * pianod_ip;
  int pianod_port;
  int pianod_fd;
  char request[1000];
  int rlen;
  private slots:
  int service_heartbeat(void);
};

/** \class MythPianod
 */
class MythPianod : public MythScreenType, public MythPianoServiceListener
{
  Q_OBJECT
  public:
    MythPianod(MythScreenStack *parent, QString name);
    virtual ~MythPianod();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    void RecvMessage(const char* message);

  private:
    void Refresh();
    MythUIText     *m_titleText;
    MythUIText     *m_songText;
    MythUIText     *m_artistText;
    MythUIText     *m_albumText;
    MythUIText     *m_playTimeText;
    MythUIText     *m_ratingText;
    MythUIText     *m_stationText;
    MythUIButton   *m_unloveBtn;
    MythUIButton   *m_logoutBtn;
    MythUIButton   *m_skipBtn;
    MythUIButton   *m_tiredBtn;
    MythUIButton   *m_loveBtn;
    MythUIButton   *m_hateBtn;
    MythUIButton   *m_stationsBtn;
    MythUIText     *m_outText;
    MythUIImage    *m_coverartImage;
      
    QHttp          *m_coverArtFetcher;
    QTemporaryFile *m_coverArtTempFile;
    QTimer         *m_Timer;

  private slots:
    QString getTimeString(int exTime, int maxTime);
    void ui_heartbeat(void);
    void coverArtFetched(void);
    void unloveCallback();
    void logoutCallback();
    void skipCallback();
    void hateCallback();
    void loveCallback();
    void tiredCallback();
    void selectStationCallback();
};


class MythPianodConfig : public MythScreenType, public MythPianoServiceListener
{
  Q_OBJECT
  public:
    MythPianodConfig(MythScreenStack *parent, QString name);
    ~MythPianodConfig();
  
    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void RecvMessage(const char* message) {
      if (m_outText)
	m_outText->SetText(QString(message));
    }

  private:
    MythUITextEdit   *m_nameEdit;
    MythUITextEdit   *m_passwordEdit;
    MythUIText       *m_outText;
    MythUIButton     *m_loginBtn;
    
  private slots:
    void loginCallback();
};

class MythPianodPopup : public MythScreenType
{
  Q_OBJECT
  public:
    MythPianodPopup(MythScreenStack *parent, QString name);
  
    bool Create(void);

  private:
    MythUIText       *m_notifyText;
    MythUIText     *m_outText;
  private slots:
};


class MythPianodStationSelect : public MythScreenType
{
  Q_OBJECT
  public:
    MythPianodStationSelect(MythScreenStack *parent, QString name);
    ~MythPianodStationSelect();
  
    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  private:
    MythUIButtonList *m_stations;    

   private slots:
    void stationSelectedCallback(MythUIButtonListItem *item);
};

#endif /* MYTHPANDORA_H */
