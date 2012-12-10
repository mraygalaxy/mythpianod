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

// POSIX headers
#include <unistd.h>
#include <assert.h>

#include <QUrl>

// MythTV headers
#include "mythuibutton.h"
#include "mythuitext.h"
#include "mythmainwindow.h"
#include "mythcontext.h"
#include "mythdirs.h"
#include "lcddevice.h"

// MythPianod headers
#include "mythpianod.h"

static int debug = 0;

MythPianoService::MythPianoService()
  : m_PlayerThread(NULL),
    m_Listener(NULL),
    m_Timer(NULL),
    current_station(-1),
    current_station_name(""),
    pianod_ip("127.0.0.1"),
    pianod_port(4445),
    pianod_fd(-1),
    response(NULL),
    duration("00:00"),
    played("00:00"),
    song_changed(0)
{
}

void MythPianoService::SetCurrentStation(QString name) {
  int x = 0;

  for(x = 0; x < stations.size(); x++) { 
	if(QString(stations[x].c_str()) == name) {
		current_station_name = stations[x];
		current_station = x;
		break;
	}
  }
}
MythPianoService::~MythPianoService()
{
    if(pianod_fd != -1)
      Logout();
}

void
MythPianoService::SetMessageListener(MythPianoServiceListener* listener)
{
  m_Listener = listener;
}

void
MythPianoService::RemoveMessageListener(MythPianoServiceListener* listener)
{
  if (listener == m_Listener)
    m_Listener = NULL;
}

void MythPianoService::BroadcastMessage(const char *format, ...)
{
  QString buffer;
  va_list args;
  va_start(args, format);
  buffer.vsprintf(format, args);
  va_end(args);

  if(debug)
  printf("**** MythPianoService: %s\n", buffer.ascii());

  if (m_Listener)
    m_Listener->RecvMessage(buffer.ascii());
  
}

void MythPianoService::PauseToggle()
{
  rlen = sprintf(request, "playpause\n");
  SendPianodRequest(200);
}

void MythPianoService::Logout()
{
  rlen = sprintf(request, "stop now\n");
  SendPianodRequest(200);
  PianodDisconnect("Exiting plugin from Pianod");
  if(debug)
  printf("Exiting plugin from Pianod\n");
}

void MythPianoService::PianodDisconnect(string msg) {
	std::cout<<msg<<endl;
	close(pianod_fd);
	pianod_fd = -1;
	if(response) {
		delete(response);
		response = NULL;
	}
}

void MythPianoService::CheckForResponse(int success1, int success2, int success3, int success4, int len) {
	if(response) {
		delete(response);
		response = NULL;
	}
	if(len > 0) {
		response = GetPianodLines(success1, success2, success3, success4);
	} else {
		perror("Failed to send pianod request");
	}
}

int MythPianoService::SendPianodRequest(int success) {
	int len = write(pianod_fd, request, rlen);
 	CheckForResponse(success, -1, -1, -1, len);
	return len;
}

static int max_response = 4000;

std::vector<MythPianoResponse> * MythPianoService::GetPianodLines(int success1, int success2, int success3, int success4)
{
	int total = 0;
	char byte;
	string line = "";
	std::vector<MythPianoResponse> * resp = new std::vector<MythPianoResponse>();

	if(pianod_fd == -1) {
		if(debug)
		printf("socket is closed. ignoring request.\n");
		return resp;
	}

	while(total++ < max_response) {
		int len = read(pianod_fd, &byte, 1);
		if(len < 0) {
			PianodDisconnect("Error getting response from pianod\n");
			perror("read");
			return resp;
		}


		if(byte == '\n') {
			int code = atoi(line.substr(0,3).c_str());
			string value = line.substr(4);
			int stop = 0;

			if(code == 101) {
				if(success1 != 101 && (success2 != -1 && success2 != 101) && (success3 != -1 && success3 != 101)&& (success4 != -1 && success4 != 101)) {
					line = "";
					if(debug)
					printf("Ignoring current track\n");
					continue;		
				}
			} else if(code == 102) {
				if(success1 != 102 && (success2 != -1 && success2 != 102) && (success3 != -1 && success3 != 102)&& (success4 != -1 && success4 != 102)) {
					line = "";
					if(debug)
					printf("Ignoring current track\n");
					continue;
				}
			} else if(code == 100) {
				line = "";
				if(debug)
				printf("Ignoring welcome.\n");
				continue;
			} else if(code == 203) {
				if(debug)
				std::cout<<"Status code: " << code << " value: " << value << endl;
			} else if(code > 100 && code < 200) {
				if(debug)
				std::cout<<"Info code: " << code << " value: " << value << endl;
			} else if(code >= 200 && code <= 299) {
				if(debug)
				std::cout<<"Success code: " << code << " value: " << value << endl;
			} else if(code >= 400 && code <= 499) {
				std::cout<<"Error code: " << code << " value: " << value << endl;
				stop = 1;
			} else {
				std::cout<<"Unknown error: " << code << " value: " << value << endl;
			}
			line = "";
			resp->push_back(MythPianoResponse(code, value));
			if(stop || (success1 == code) || (success2 != -1 && success2 == code) || (success3 != -1 && success3 == code)|| (success4 != -1 && success4 == code))
				break;
			if(debug)
			printf("Not stopping: %d != %d\n", code, success1);	
					
		} else {
			line += byte;
		}
	}

	if(total >= max_response) {
		resp->push_back(MythPianoResponse(400, "Response was too big\n"));
		PianodDisconnect("Response is too big. Assuming Error\n");
		return resp;
	}	

	return resp;
}

int MythPianoService::RepopulateStations() {
  BroadcastMessage("Retrieving station list...\n");
  stations.clear();
  rlen = sprintf(request, "stations list\n");
  SendPianodRequest(204);
  if(response->back().code == 204) {
	  for(int x = 0; x < response->size(); x++) {
	     MythPianoResponse r = response->at(x);
	     if(r.code != 115)
		continue;
	     /* remove the "Station: " */
	     r.value = r.value.substr(9);
	     std::cout<<"Adding station: " << r.value << endl;
	     stations.push_back(r.value);
	  }
  } else {
     PianodDisconnect("Failed to retrieve station list. Bailing: " + response->back().value);
     return -1;
  }
  service_heartbeat();
  return 0;
}

int MythPianoService::Login()
{
  QString username = gCoreContext->GetSetting("pandora-username");
  QString password = gCoreContext->GetSetting("pandora-password");

  //wtf really?
  char* usernameBuff = strndup(username.toUtf8().data(), 1024);
  char* passwordBuff = strndup(password.toUtf8().data(), 1024);

  if(pianod_fd != -1) {
	if(debug)
	printf("Already connected.\n");
	if(RepopulateStations() < 0)
		return -1;
	return 0;
  }

  int fd = -1;

  if((fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
	perror("could not open pianod socket");
  	return -1;
  }

  bzero(&pianod_addr, sizeof(pianod_addr));
  pianod_addr.sin_family = AF_INET;
  pianod_addr.sin_addr.s_addr = inet_addr(pianod_ip);
  pianod_addr.sin_port = htons(pianod_port);

  if(::connect(fd, (struct sockaddr *) &pianod_addr, sizeof(struct sockaddr_in)) < 0) { 
 	perror("could not connect to pianod server");
	return -1;
  }

  BroadcastMessage("Connecting to pianod...\n");
  pianod_fd = fd;
  response = GetPianodLines(200, -1, -1, -1);
  int responses = response->size();

  if(responses != 1) {
	printf("More than one pianod response: %d\n", responses);
	PianodDisconnect("too many initial pianod responses");
	return -1;
  }

  if((*response)[0].code != 200) {
	PianodDisconnect("Non-successful attempt on initial connection: " + (*response)[0].code + (*response)[0].value);
	return -1;
  }

  BroadcastMessage("Authenticating with pianod...\n");
  rlen = sprintf(request, "user %s %s\n", usernameBuff, passwordBuff); 
  SendPianodRequest(200);

  if(response->back().code != 200) {
	PianodDisconnect("Authentication failed: " + response->back().value);
	return -1;
  }

  BroadcastMessage("Connected to pianod.\n");

  // wtf
  free(usernameBuff);
  free(passwordBuff);

  if(RepopulateStations() < 0) {
	return -1;
  }

  return 0;
}

map<string, string> MythPianoService::PullOutSong(int idx) 
{
	map<string, string> song;

	  for(idx; idx < response->size(); idx++) {
	     MythPianoResponse r = response->at(idx);
	     if(r.code == 204 or r.code == 203) {
	              break;
	     } else {
		     /* remove the "Station: " */
		     size_t pos = r.value.find(": ");
		     string key = r.value.substr(0, pos);
		     string value = r.value.substr(pos + 2); 
		     if(debug)
		     std::cout<<"Song: " + key + " = " + value << endl;
		     song[key] = value;
	     }
	  }
	return song;
}

int MythPianoService::GetPlaylist()
{
  BroadcastMessage("Getting playlist...\n");
  rlen = sprintf(request, "queue\n");
  SendPianodRequest(204);

  /* parse play list from queue command */
  if(response->back().code == 204) {
	  playlist.clear();
	  for(int x = 0; x < response->size() - 1; x++) {
	     MythPianoResponse r = response->at(x);
	     if(response->at(x).code == 203) {
		  map<string, string> song = PullOutSong(x + 1);
 		  if(song.size() != 0) {
		      cout<<"Storing new song: " << song["Title"] << endl;
		      playlist.push_back(song);
		  }
	     }
	  }
  } else {
	BroadcastMessage("Failed to get playlist(2)!\n");
        return -1;
  }

  if(service_heartbeat() < 0) {
	BroadcastMessage("service heartbeat failed\n");
	return -1;
  }
 
  return 0;
  
}

void MythPianoService::StartPlayback()
{
  BroadcastMessage("Starting playback... \n");
  
  rlen = sprintf(request, "stop now\n");
  SendPianodRequest(200);
  rlen = sprintf(request, "select station \"%s\"\n", stations[current_station].c_str());
  SendPianodRequest(200);
  rlen = sprintf(request, "play\n");
  SendPianodRequest(200);
  if(response->back().code != 200) {
	BroadcastMessage("Failed to start playback!\n");
	return;
  }


  if (playlist.size() == 0) {
    BroadcastMessage("Empty playlist");
    return;
  }
}

void MythPianoService::StartPlayerThread() {
  if (m_Timer) {
    m_Timer->stop();
    m_Timer->disconnect();
    delete m_Timer;
  }
  m_Timer = new QTimer(this);
  connect(m_Timer, SIGNAL(timeout()), this, SLOT(service_heartbeat()));
  m_Timer->start(1000);
}

void MythPianoService::StopPlayerThread() {
  if (m_Timer) {
    m_Timer->stop();
    m_Timer->disconnect();
    delete m_Timer;
    m_Timer = NULL;
  }
}

void
MythPianoService::StopPlayback()
{

  rlen = sprintf(request, "stop now\n");
  SendPianodRequest(200);
}

int
MythPianoService::Volume()
{
  // 0-100;
   /* Need to implement... */
  return 0;
}

void
MythPianoService::VolumeUp()
{
   /* Need to implement... */
}

void
MythPianoService::VolumeDown()
{
   /* Need to implement... */
}

void
MythPianoService::ToggleMute()
{
   /* Need to implement... */
}

void
MythPianoService::NextSong()
{
  BroadcastMessage("Sending skip...\n");
  rlen = sprintf(request, "skip\n");
  SendPianodRequest(200);
}

int
MythPianoService::service_heartbeat()
{
  if(debug)
  printf("Determining current song...\n");
  rlen = sprintf(request, "status\n");
  SendPianodRequest(204);
 
  /* parse current song */
  if(response->back().code == 204) {
	  for(int x = 0; x < response->size() - 1; x++) {
	     MythPianoResponse r = response->at(x);
	     if(response->at(x).code == 203) {
		  map<string, string> song = PullOutSong(x + 1);
 		  if(song.size() != 0) {
		      if(debug)
		      cout<<"Setting current song: " << song["Title"] << endl;
		      if(!current_song.size() || (current_song["Title"] != song["Title"])) {
			      current_song = song; 
			      song_changed = 1;
		      } else {
			current_song["Rating"] = song["Rating"];
			}
			
		  }
	     }
	  }
  } else {
	BroadcastMessage("Failed to get current song(2)!\n");
        return -1;
  }

  if(debug)
  printf("Checking for status....current_station %d \n", current_station);
  CheckForResponse(101, 102, 103, 104, 1);
  MythPianoResponse r = response->back();

  if(r.code == 101 || r.code == 102 || r.code == 104) {
	size_t pos = r.value.find("/");
 	played = r.value.substr(0, pos);
	string rest = r.value.substr(pos + 1);
	pos = rest.find("/");
	duration = rest.substr(0, pos);
	if(current_station == -1) {
		pos = r.value.find(" ");
		rest = r.value.substr(pos + 1);
		pos = rest.find(" ");
		rest = rest.substr(pos + 1);
		pos = rest.find(" ");
		current_station_name = rest.substr(pos + 1);
		SetCurrentStation(QString(current_station_name.c_str()));
		if(debug)
		cout << "current station is " + current_station_name << " " << current_station << endl;
	}
  } else if(r.code == 103) {
	if(debug)
	printf("pianod is stopped\n");
  }  else {
	BroadcastMessage("Failed to get duration of current song\n");
	return -1;
  }

 return 0; 
}

void MythPianoService::GetTimes(string *play, string *dur)
{
    *play   = played;
    *dur = duration;
};


/** \brief Creates a new MythPianod Screen
 *  \param parent Pointer to the screen stack
 *  \param name The name of the window
 */
MythPianod::MythPianod(MythScreenStack *parent, QString name) :
  MythScreenType(parent, name),
  m_coverArtFetcher(NULL),
  m_coverArtTempFile(NULL),
  m_Timer(NULL)
{
  //example of how to find the configuration dir currently used.
  QString confdir = GetConfDir();
  LOG(VB_GENERAL, LOG_INFO, "MythPianod Conf dir:"  + confdir);
}

MythPianod::~MythPianod()
{
  MythPianoService* service = GetMythPianoService();
  service->RemoveMessageListener(this);
  service->StopPlayerThread();

  if (m_coverArtTempFile)
    delete m_coverArtTempFile;

  if (m_coverArtFetcher)
    delete m_coverArtFetcher;
}

void MythPianod::Refresh() {
    MythPianoService* service = GetMythPianoService();
    if (service->GetCurrentSong().size()) {
      if(service->SongChanged()) {
	      m_songText->SetText(QString(service->GetCurrentSong()["Title"].c_str()));
	      m_artistText->SetText(QString(service->GetCurrentSong()["Artist"].c_str()));
	      m_albumText->SetText(QString(service->GetCurrentSong()["Album"].c_str()));
              m_stationText->SetText(QString(service->GetCurrentStation().c_str()));

	      // kick off cover art load
	      if (m_coverArtFetcher)
		delete m_coverArtFetcher;

	      if (m_coverArtTempFile)
		delete m_coverArtTempFile;

	      m_coverArtFetcher = new QHttp();
	      connect(m_coverArtFetcher, SIGNAL(done(bool)), this, SLOT(coverArtFetched()));  
	      QUrl u(service->GetCurrentSong()["CoverArt"].c_str());
	      QHttp::ConnectionMode conn_mode = QHttp::ConnectionModeHttp;
	      m_coverArtFetcher->setHost(u.host(), conn_mode, 80);
	      QByteArray path = QUrl::toPercentEncoding(u.path(), "!$&'()*+,;=:@/");
	      m_coverArtFetcher->get(path);
      }
	  string played, duration;
	  service->GetTimes(&played, &duration);
	  string rating = service->GetCurrentSong()["Rating"];
	  if(rating == "good") {
		  m_ratingText->SetText(QString("This song makes me warm and fuzzy inside!"));
	  } else if(rating == "bad") {
		  m_ratingText->SetText(QString("Terrible song! Make it stop! Ahhh..."));
	  } else if(rating == "") {
		  m_ratingText->SetText(QString("Can't put my finger on this song yet..."));
	  } else {
		cout<<"unknown rating: " << rating << endl;
	  }
	  if(played == "Intertrack") {
		  QString time_string("00:00 / 00:00 Loading next track...");
		  m_playTimeText->SetText(time_string);
	  } else {
		  QString time_string((played + " / " + duration).c_str());
		  m_playTimeText->SetText(time_string);
	  }

    }
}

void
MythPianod::RecvMessage(const char* message) {
  if (!strcmp(message, "New Song")) {
	Refresh();
  }
  else if (m_outText)
    m_outText->SetText(QString(message));
}

void
MythPianod::coverArtFetched(void)
{
  QByteArray array = m_coverArtFetcher->readAll();

  m_coverArtTempFile = new QTemporaryFile();
  m_coverArtTempFile->open();
  m_coverArtTempFile->write(array);
  m_coverArtTempFile->flush();
  m_coverArtTempFile->waitForBytesWritten(-1);
  m_coverArtTempFile->close();

  QString filename = m_coverArtTempFile->fileName();

  m_coverartImage->SetFilename(filename);
  m_coverartImage->Load();
}


bool MythPianod::Create(void)
{
  bool foundtheme = false;

  // Load the theme for this screen
  foundtheme = LoadWindowFromXML("pandora-ui.xml", "pandora", this);
    
  if (!foundtheme)
    return false;

  bool err = false;
  UIUtilE::Assign(this, m_titleText,  "title", &err);
  UIUtilE::Assign(this, m_outText,    "outtext", &err);
  UIUtilE::Assign(this, m_songText,   "song", &err);
  UIUtilE::Assign(this, m_artistText, "artist", &err);
  UIUtilE::Assign(this, m_albumText,  "album", &err);
  UIUtilE::Assign(this, m_playTimeText,  "playtime", &err);
  UIUtilE::Assign(this, m_ratingText,  "rating", &err);
  UIUtilE::Assign(this, m_coverartImage, "coverart", &err);
  UIUtilE::Assign(this, m_logoutBtn,     "logoutBtn", &err);
  UIUtilE::Assign(this, m_unloveBtn,     "unloveBtn", &err);
  UIUtilE::Assign(this, m_skipBtn,     "skipBtn", &err);
  UIUtilE::Assign(this, m_hateBtn,     "hateBtn", &err);
  UIUtilE::Assign(this, m_loveBtn,     "loveBtn", &err);
  UIUtilE::Assign(this, m_tiredBtn,     "tiredBtn", &err);
  UIUtilE::Assign(this, m_stationsBtn,   "stationsBtn", &err);
  UIUtilE::Assign(this, m_stationText,   "stationname", &err);

  if (err) {
    LOG(VB_GENERAL, LOG_INFO, "Cannot load screen 'pandora'");
    return false;
  }

  connect(m_logoutBtn, SIGNAL(Clicked()), this, SLOT(logoutCallback()));
  connect(m_unloveBtn, SIGNAL(Clicked()), this, SLOT(unloveCallback()));
  connect(m_skipBtn, SIGNAL(Clicked()), this, SLOT(skipCallback()));
  connect(m_tiredBtn, SIGNAL(Clicked()), this, SLOT(tiredCallback()));
  connect(m_loveBtn, SIGNAL(Clicked()), this, SLOT(loveCallback()));
  connect(m_hateBtn, SIGNAL(Clicked()), this, SLOT(hateCallback()));
  connect(m_stationsBtn, SIGNAL(Clicked()), this, SLOT(selectStationCallback()));

  BuildFocusList();

  SetFocusWidget(m_coverartImage);
  // dummy image needed
  m_coverartImage->SetFilename("/usr/share/app-install/icons/_usr_share_icons_hicolor_scalable_apps_emacs23.png");
  m_coverartImage->Load();

  MythPianoService* service = GetMythPianoService();

  service->SetMessageListener(this);

  service->StartPlayerThread();

  m_Timer = new QTimer(this);
  connect(m_Timer, SIGNAL(timeout()), this, SLOT(ui_heartbeat()));
  m_Timer->start(1000);

  return true;
}


void
MythPianod::ui_heartbeat(void)
{
  Refresh();
  if(debug)
  printf("MythPianod heartbeat finished.........\n");
}


bool MythPianod::keyPressEvent(QKeyEvent *event)
{
  if (GetFocusWidget() && GetFocusWidget()->keyPressEvent(event))
    return true;
    
  bool handled = false;
  QStringList actions;
  handled = GetMythMainWindow()->TranslateKeyPress("MythPianod", event, actions);

  for (int i = 0; i < actions.size() && !handled; i++) {
    QString action = actions[i];
    handled = true;
    
    if (action == "ESCAPE") {
        MythPianoService* service = GetMythPianoService();
        /* If the player screen is loaded, then just show the Station screen, otherwise, stop everything and go to the home screen */
	if(GetScreenStack()->GetTopScreen() != this) {
           GetScreenStack()->PopScreen(false, true);
	  service->StopPlayback();
	  service->StopPlayerThread();
	} else {
           GetScreenStack()->PopScreen(false, true);
	   showStationSelectDialog();
	  	
	}
    }
    else if (action == "NEXTTRACK" || action == "RIGHT" || action == "PAGEDOWN")
    {
        MythPianoService* service = GetMythPianoService();
        service->NextSong();
    }
    else if (action == "PAUSE" || action == "PLAY")
    {
	  MythPianoService* service = GetMythPianoService();
	  service->PauseToggle();
          handled = true;
    
    } else if (action == "SELECT") {
       if(debug)
       printf("What does select keypress do?\n");
	handled = true;
 
    } else if (action == "MUTE")
	{
	  // XXX UI needed
	  MythPianoService* service = GetMythPianoService();
	  service->ToggleMute();
	}
    else if (action == "VOLUMEDOWN")
	{
	  // XXX UI needed
	  MythPianoService* service = GetMythPianoService();
	  service->VolumeDown();
	}
    else if (action == "VOLUMEUP")
	{
	  // XXX UI needed
	  MythPianoService* service = GetMythPianoService();
	  service->VolumeUp();
	}
    else {
       if(debug)
      printf("Unknown keypress %s\n", action.toAscii().data());
      handled = false;
    }
  }

  if (!handled && MythScreenType::keyPressEvent(event))
    handled = true;

  return handled;
}

void MythPianod::logoutCallback()
{
  // Not really sure if we should blow these away.  Instead, just go back to the login dialog.
  //  gCoreContext->SaveSetting("pandora-username", QString(""));
  //  gCoreContext->SaveSetting("pandora-password", QString(""));

  MythPianoService* service = GetMythPianoService();
  service->StopPlayback();
  service->StopPlayerThread();
  service->Logout();

  GetScreenStack()->PopScreen(false, true);
  showLoginDialog();
}

void MythPianod::unloveCallback()
{
  GetMythPianoService()->UnloveSong();
}

void MythPianod::skipCallback()
{
  GetMythPianoService()->SkipSong();
}
void MythPianod::loveCallback()
{
  GetMythPianoService()->LoveSong();
}
void MythPianod::hateCallback()
{
  GetMythPianoService()->HateSong();
  GetMythPianoService()->SkipSong();
}

void MythPianod::tiredCallback()
{
  GetMythPianoService()->TiredSong();
  GetMythPianoService()->SkipSong();
}

void MythPianod::selectStationCallback()
{
  GetScreenStack()->PopScreen(false, true);
  showStationSelectDialog();
}

MythPianodConfig::MythPianodConfig(MythScreenStack *parent, QString name)
    : MythScreenType(parent, name)
{
}

MythPianodPopup::MythPianodPopup(MythScreenStack *parent, QString name)
    : MythScreenType(parent, name)
{
}

bool MythPianodConfig::Create()
{
  bool foundtheme = false;

  // Load the theme for this screen
  foundtheme = LoadWindowFromXML("pandora-ui.xml", "pandorasettings", this);

  if (!foundtheme)
    return false;

  bool err = false;
  UIUtilE::Assign(this, m_nameEdit,    "username", &err);
  UIUtilE::Assign(this, m_passwordEdit,"password", &err);
  UIUtilE::Assign(this, m_loginBtn,    "loginBtn", &err);
  UIUtilE::Assign(this, m_outText,     "outtext",  &err);
  if (err) {
    LOG(VB_GENERAL, LOG_INFO, "Cannot load screen 'pandora'");
    return false;
  }

  connect(m_loginBtn, SIGNAL(Clicked()), this, SLOT(loginCallback()));

  BuildFocusList();

  m_passwordEdit->SetPassword(true);

  QString username = gCoreContext->GetSetting("pandora-username");
  QString password = gCoreContext->GetSetting("pandora-password");

  m_nameEdit->SetText(username);
  m_passwordEdit->SetText(password);

  MythPianoService* service = GetMythPianoService();
  service->SetMessageListener(this);

  return true;
}

bool MythPianodPopup::Create()
{
  bool foundtheme = false;

  // Load the theme for this screen
  foundtheme = LoadWindowFromXML("pandora-ui.xml", "popup", this);

  if (!foundtheme)
    return false;

  bool err = false;
  UIUtilE::Assign(this, m_notifyText, "notification",  &err);
  UIUtilE::Assign(this, m_outText,    "outtext", &err);
  if (err) {
    LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'popup'");
    return false;
  }

  //BuildFocusList();
  m_notifyText->SetText("something else to say ....");
  return true;
}

MythPianodConfig::~MythPianodConfig()
{
  MythPianoService* service = GetMythPianoService();
  service->RemoveMessageListener(this);
}

bool MythPianodConfig::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void MythPianodConfig::loginCallback()
{
  gCoreContext->SaveSetting("pandora-username", m_nameEdit->GetText());
  gCoreContext->SaveSetting("pandora-password", m_passwordEdit->GetText());

  MythPianoService* service = GetMythPianoService();
  if (service->Login() == 0) {
	GetScreenStack()->PopScreen(false, true);
	if(service->GetCurrentStation() == "") {
		showStationSelectDialog();
	} else {
		showPlayerDialog();
	}
  }
}


MythPianodStationSelect::MythPianodStationSelect(MythScreenStack *parent, QString name)
  : MythScreenType(parent, name)
{
}

MythPianodStationSelect::~MythPianodStationSelect()
{
}

bool
MythPianodStationSelect::Create(void)
{
  bool foundtheme = false;

  // Load the theme for this screen
  foundtheme = LoadWindowFromXML("pandora-ui.xml", "pandorastations", this);

  if (!foundtheme)
    return false;

  bool err = false;
  UIUtilE::Assign(this, m_stations, "stations", &err);

  if (err) {
    LOG(VB_GENERAL, LOG_INFO, "Cannot load screen 'pandora'");
    return false;
  }

  BuildFocusList();

  MythPianoService* service = GetMythPianoService();
  vector<string> stations = service->GetStations();

  for(int x = 0; x < stations.size(); x++) {
    MythUIButtonListItem* item = new MythUIButtonListItem(m_stations, QString(stations[x].c_str()));
    item->SetData(QString(stations[x].c_str()));
  }

  connect(m_stations, SIGNAL(itemClicked(MythUIButtonListItem*)),
	  this, SLOT(stationSelectedCallback(MythUIButtonListItem*)));

  return true;
}

bool
MythPianodStationSelect::keyPressEvent(QKeyEvent *event)
{
  if (GetFocusWidget()->keyPressEvent(event))
    return true;
  
  bool handled = false;
  
  if (!handled && MythScreenType::keyPressEvent(event))
    handled = true;
  
  return handled;
}

void
MythPianodStationSelect::stationSelectedCallback(MythUIButtonListItem *item)
{
  GetMythPianoService()->SetCurrentStation(item->GetData().toString());
  GetMythPianoService()->StartPlayback();
  
  GetScreenStack()->PopScreen(false, true);
  showPlayerDialog();
}
