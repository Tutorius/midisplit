 // #define STOPIT 1
 // #define PRINTIT 1

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

char s[80];
char hs[80];
uint8_t *Tracks[65536];
uint32_t Lens[65536];
uint32_t Notes[65536];

uint32_t header_length;
uint16_t header_format,header_nt,header_division;

uint32_t track_length;

uint32_t eventpointer;

char TrackName[200];
char CopyRight[200];
char Text[200];

uint8_t Note;
uint8_t Velocity;
uint8_t Program;
uint8_t Controller;
uint8_t Value;
uint8_t AfterTouch;
uint8_t PitchBig;
uint8_t PitchSmall;
uint8_t MidiChannel;

uint32_t ActTime;

typedef struct note
{
	uint8_t Note;
	uint8_t Velocity;
	uint32_t Starttime;
	uint32_t Endtime;
	
	uint8_t MonoTrack; // Track, auf den diese Note auszugeben ist
	
	uint8_t Event;
	uint8_t Len;
	unsigned char EventData[300];
	
	int mode; // 0 -> Note; 1 -> Event
	
	struct note *Previous;
	struct note *Next;
} NOTE;

NOTE *notestart[65536],*noteend[65536];
NOTE *lastnote;
NOTE *prenote;
NOTE *actnote;

uint8_t MaxPolyphony[65536];


void HexPrint(unsigned char b,char *s)
{
	int i;
	sprintf(s,"%2x",b);
	for (i=0;i<strlen(s);i++)
	{
		if (s[i]==32) s[i]='0';
	}
}

uint32_t ReadVariable(char *s)
{
	uint32_t l,l1,eventpointer2;
	int i,flag;
	char s1[1000];
	l=0;
	flag=true;
	eventpointer2=eventpointer;
	do
	{
#ifdef PRINTIT
		HexPrint(s[eventpointer],s1); printf("%s|",s1);
#endif
		if (((uint32_t)s[eventpointer])>127)
		{
			l1=((uint32_t)s[eventpointer])&0x7F;
			l+=l1;
			l=l<<7;
			eventpointer++;
		}
		else
		{
			l1=((uint32_t)s[eventpointer]);
			l+=l1;
			flag=false;
			eventpointer++;
		}
	} while (flag);
#ifdef PRINTIT
	printf(" -> ReadVariable %ld\n",l);
#endif
	// printf("ReadVariable %ld Eventpointer von %ld bis %ld\n",l,eventpointer2,eventpointer);
	return(l);
}

int Write32BE(FILE *file1,uint32_t a)
{
  unsigned char b[4];
  b[0]=(uint32_t)a >> 24;
  b[1]=(uint32_t)a >> 16;
  b[2]=(uint32_t)a >> 8;
  b[3]=(uint32_t)a;
  return(fwrite(b,4,1,file1));
}

int Read32BE(FILE *file1,uint32_t *a)
{
 unsigned char b[4],dummy;
 int ln;
 uint32_t val;
 ln=fread(b,4,1,file1);
 val=((uint32_t)b[0]<<24) | ((uint32_t)b[1]<<16) | ((uint32_t)b[2]<<8) | ((uint32_t)b[3]);
 *a=val;
 return(ln);
}

int Write16BE(FILE *file1,uint16_t a)
{
  unsigned char b[2];
  b[0]=(uint32_t)a >> 8;
  b[1]=(uint32_t)a;
  return(fwrite(b,2,1,file1));
}

int Read16BE(FILE *file1,uint16_t *a)
{
 unsigned char b[2],dummy;
 int ln;
 uint16_t val;
 ln=fread(b,2,1,file1);
 val=((uint16_t)b[0]<<8) | ((uint16_t)b[1]);
 *a=val;
 return(ln);
}

void WriteVariable(uint32_t value,unsigned char *s,uint8_t *len)
{
	uint8_t i,j;
	unsigned char buffer[10];
	uint32_t value2;
	uint8_t val;
	value2=value;
	i=0;
	buffer[i++]=value2&0x7F;
	value2&=0xFFFFFF80;
	value2=value2>>7;
	while(value2>0)
	{
		val=(uint8_t)(value2&0x7F);
		buffer[i++]=val|0x80;
		value2&=0xFFFFFF80;
		value2=value2>>7;
	}
	*len=i;
	for (j=0;j<i;j++)
	{
		s[j]=buffer[i-j-1];
	}
}

uint32_t WriteMonoTrack(FILE *file1,int track,int write,int ActTrack)
{
	uint8_t i;
	uint32_t length;
	uint8_t len;
	unsigned char s1[100],s2[100];
	uint32_t acttime;
	uint8_t event;
	NOTE *snote;
	length=0;
	acttime=0;
	snote=notestart[ActTrack];
	do
	{
		/*if (ActTrack==3)
		{
			printf("Mode %d MonoTrack %d Event %x ..",snote->mode,snote->MonoTrack,snote->Event);
		}*/
		if ((snote->mode==0)&&(snote->MonoTrack==track+1)&&(((snote->Event&0xF0)==0x90)))
		{
			WriteVariable(snote->Starttime-acttime,s1,&len);
			s1[len++]=snote->Event;
			s1[len++]=snote->Note;
			s1[len++]=snote->Velocity;
			if (write)
				fwrite(s1,1,len,file1);
			length+=len;
			#ifdef PRiNTIT
			printf("%lx\n",length);
			#endif
			acttime=snote->Starttime;
			WriteVariable(snote->Endtime-acttime,s1,&len);
			event=snote->Event;
			event&=0x0F;
			event|=0x80;
			s1[len++]=event;
			s1[len++]=snote->Note;
			s1[len++]=snote->Velocity;
			if (write)
				fwrite(s1,1,len,file1);
			length+=len;
			#ifdef PRINTIT
			printf("%lx\n",length);
			#endif
			acttime=snote->Endtime;
		}
		if(snote->mode==1)
		{
			if (snote->Starttime==0)
			{
				switch(snote->Event)
				{
					case 0x03:
					case 0x02:
					case 0x04:
					case 0x05:
					case 0x06:
					case 0x07:
					case 0x01:
					case 0x08:
					case 0x09:
					case 0x7F:
						WriteVariable(snote->Starttime-acttime,s1,&len);
						s1[len++]=0xFF;
						s1[len++]=snote->Event;
						if (write)
							fwrite(s1,1,len,file1);
						length+=len;
						#ifdef PRINTIT
						printf("%lx\n",length);
						#endif
						s1[0]=0; len=0;
						if (write)
							fwrite(&snote->Len,1,1,file1);
						length++;
						if (write)
						{
							fwrite(snote->EventData,1,snote->Len,file1);
						}
						length+=snote->Len;
						#ifdef PRINTIT
						printf("%lx\n",length);
						#endif
						break;
					case 0x51:
						WriteVariable(snote->Starttime-acttime,s1,&len);
						s1[len++]=0xFF;
						s1[len++]=snote->Event;
						if (write)
							fwrite(s1,1,len,file1);
						length+=len;
						#ifdef PRINTIT
						printf("%lx\n",length);
						#endif
						s1[0]=0; len=0;
						if (write)
							fwrite(snote->EventData,1,snote->Len,file1);
						length+=snote->Len;
						#ifdef PRINTIT
						printf("%lx\n",length);
						#endif
						break;
					case 0x58:
					case 0x59:
						WriteVariable(snote->Starttime-acttime,s1,&len);
						s1[len++]=0xFF;
						s1[len++]=snote->Event;
						if (write)
							fwrite(s1,1,len,file1);
						length+=len;
						#ifdef PRINTIT
						printf("%lx\n",length);
						#endif
						s1[0]=0; len=0;
						if (write)
						{
							fwrite(&snote->Len,1,1,file1);
							fwrite(snote->EventData,1,snote->Len,file1);
						}
						length++;
						length+=snote->Len;
						#ifdef PRINTIT
						printf("%lx\n",length);
						#endif
						break;
					case 0x2F:
						WriteVariable(snote->Starttime-acttime,s1,&len);
						s1[len++]=0xFF;
						s1[len++]=snote->Event;
						if (write)
							fwrite(s1,1,len,file1);
						length+=len;
						#ifdef PRINTIT
						printf("%lx\n",length);
						#endif
						s1[0]=0; len=0;
						s1[0]=0x2F; s1[1]=0;
						if (write)
							fwrite(s1,1,2,file1);
						
						length+=2;
						#ifdef PRINTIT
						printf("%lx\n",length);
						#endif
						break;
				} 
			}
		}
		snote=snote->Next;
	}
	while (snote!=NULL);
	return(length);
}

void SplitPolyphony(int ActTrack)
{
	int i,flag,flag2,flag3;
	NOTE *snote,*tnote;
	char s1[100];
	for (i=1;i<200;i++)
	{
		flag3=false;
		snote=notestart[ActTrack];
		flag=true;
		do
		{
			if(flag)
			{
				if ((snote->MonoTrack==0)&&(snote->mode==0))
				{
					snote->MonoTrack=i;
					flag=false;
					flag3=true;
				}
			}
			else
			{
				if ((snote->MonoTrack==0)&&(snote->mode==0))
				{
					tnote=snote->Previous;
					flag2=true;
					do
					{
						if (tnote!=NULL)
						{
							if((tnote->mode==0)&&(tnote->MonoTrack==i))
							{
							#ifdef PRINTIT
								printf("NEWNOTE START %ld END %ld\n",snote->Starttime,snote->Endtime);
								printf("OLDNOTE START %ld END %ld\n",tnote->Starttime,tnote->Endtime);
							#endif	
								if (((snote->Starttime>=tnote->Starttime)&&(snote->Starttime<=tnote->Endtime))||((snote->Endtime>=tnote->Starttime)&&(snote->Endtime<=tnote->Endtime)))
								{
									flag2=false;
								#ifdef PRINTIT
									printf("Überlappt!\n");
								#endif
								}
								//scanf("%s",s1);
							}
							tnote=tnote->Previous;
						}
					} while(tnote!=NULL);
					if (flag2)
					{
						snote->MonoTrack=i;
					#ifdef PRINTI
						printf("Snote %d\n",i);
					#endif
						flag3=true;
					}	
				}
			}
			snote=snote->Next;
		} while(snote!=NULL);
		if (!flag3)
		{
			MaxPolyphony[ActTrack]=i-1;
			break;
		}
	}
}

int ReadHeader(FILE *file1)
{
  if (fread(s,4,1,file1)==1)
  {
      s[4]=0;
      #ifdef PRINTIT
      	printf("%s ",s);
      #endif
      if (strcmp(s,"MThd")!=0)
      {
          return(false);
      }
      else
      {
         if (Read32BE(file1,&header_length)==1)
         {
             #ifdef PRINTIT
             printf("hl ");
             #endif
             if (Read16BE(file1,&header_format)==1)
             {
                 #ifdef PRINTIT
                 printf("hf ");
                 #endif
                 if (Read16BE(file1,&header_nt)==1)
                 {
                     #ifdef PRINTIT
                     printf("hn ");
                     #endif
                     if (Read16BE(file1,&header_division)==1)
                     {
                         #ifdef PRINTIT
                         printf("hd\n");
                         #endif
                         return(true);
                     }
                     else return(false);
                 }
                 else return(false);
             }
             else return(false);
         }
         else return(false);     
      }
  }
  else return(false);
}

int ReadTrackStart(FILE *file1)
{
	if (fread(s,4,1,file1)==1)
	{
		s[4]=0;
		#ifdef PRINTIT
		printf("%s ",s);
		#endif
		if (strcmp(s,"MTrk")!=0)
		{
			return(false);
		}
		else
		{
			if (Read32BE(file1,&track_length)==1)
			{
				#ifdef PRINTIT
				printf("tl\n");
				#endif
				return(true);
			}
		}
	}
	else
		return(false);
}



NOTE *SearchNote(uint8_t Note,int ActTrack)
{
	NOTE *snote,*actnote;
	snote=noteend[ActTrack];
	actnote=NULL;
	do
	{
		if (snote!=NULL)
		{
			if (snote->Note==Note)
			{
				actnote=snote;
				snote=NULL;
			}
			else
			{
				snote=snote->Previous;
			}
		}
	} while (snote!=NULL);
	return(actnote);
}

void OutputTrackInfo(char *s,int ActTrack)
{
	uint32_t time;
	uint16_t i;
	uint8_t event1,event2;
	uint8_t len1,len2,data1,data2;
	uint8_t MidiType;
	char s1[1000];
	time=ReadVariable(s);
	ActTime+=time;
	event1=(uint8_t)s[eventpointer++];
	#ifdef PRINTIT
	HexPrint(event1,s1); printf("%s|",s1);
	#endif
	if (event1==0xFF)
	{
		event2=(uint8_t)s[eventpointer++];
		#ifdef PRINTIT
		HexPrint(event2,s1); printf("%s|",s1);
		#endif
		switch(event2)
		{
			case 0x03:
				// Track-Name
				len1=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(len1,s1); printf("%s|",s1);
				#endif
				for (i=0;i<len1;i++)
				{
					TrackName[i]=s[eventpointer++];
					#ifdef PRINTIT
					HexPrint(TrackName[i],s1); printf("%s|",s1);
					#endif
				}
				TrackName[++i]=0;
				#ifdef PRINTIT
				printf(" -> Trackname %s\n",TrackName);
				#endif
				prenote=lastnote;
				lastnote=malloc(sizeof(NOTE));
				if(notestart[ActTrack]==NULL) notestart[ActTrack]=lastnote;
				noteend[ActTrack]=lastnote;
				lastnote->mode=1;
				lastnote->Event=event2;
				lastnote->Starttime=ActTime;
				lastnote->Endtime=0;
				strcpy(lastnote->EventData,TrackName);
				lastnote->Len=strlen(TrackName);
				lastnote->Previous=prenote;
				lastnote->Next=NULL;
				if (prenote!=NULL)
					prenote->Next=lastnote;
				
				break;
			case 0x02:
				// Copyright
				len1=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(len1,s1); printf("%s|",s1);
				#endif
				for (i=0;i<len1;i++)
				{
					CopyRight[i]=s[eventpointer++];
					#ifdef PRINTIT
					HexPrint(CopyRight[i],s1); printf("%s|",s1);
					#endif
				}
				CopyRight[++i]=0;
				#ifdef PRINTIT
				printf(" -> Copyright %s\n",CopyRight);
				#endif
				prenote=lastnote;
				lastnote=malloc(sizeof(NOTE));
				if(notestart[ActTrack]==NULL) notestart[ActTrack]=lastnote;
				noteend[ActTrack]=lastnote;
				lastnote->mode=1;
				lastnote->Event=event2;
				lastnote->Starttime=ActTime;
				lastnote->Endtime=0;
				strcpy(lastnote->EventData,CopyRight);
				lastnote->Len=strlen(CopyRight);
				lastnote->Previous=prenote;
				lastnote->Next=NULL;
				if (prenote!=NULL)
					prenote->Next=lastnote;

				break;
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
			case 0x01:
			case 0x08:
			case 0x09:
			case 0x7F:
				//InstrumentName, Lyric, Marker, CuePoint, Text, ProgramName, DeviceName, SequencerSpecific
				len1=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(len1,s1); printf("%s|",s1);
				#endif
				for (i=0;i<len1;i++)
				{
					Text[i]=s[eventpointer++];
					#ifdef PRINTIT
					HexPrint(Text[i],s1); printf("%s|",s1);
					#endif
				}
				Text[++i]=0;
				#ifdef PRINTIT
				printf(" -> Div. Text %s\n",Text);
				#endif
				prenote=lastnote;
				lastnote=malloc(sizeof(NOTE));
				if(notestart[ActTrack]==NULL) notestart[ActTrack]=lastnote;
				noteend[ActTrack]=lastnote;
				lastnote->mode=1;
				lastnote->Event=event2;
				lastnote->Starttime=ActTime;
				lastnote->Endtime=0;
				strcpy(lastnote->EventData,Text);
				lastnote->Len=strlen(Text);
				lastnote->Previous=prenote;
				lastnote->Next=NULL;
				if (prenote!=NULL)
					prenote->Next=lastnote;
				break;
			case 0x00:
				// Sequence-Number
				if ((uint8_t)s[eventpointer++]>0)
				{
					for (i=0;i<4;i++)
					{
						#ifdef PRINTIT
						HexPrint(s[eventpointer++],s1); printf("%s|",s1);
						#endif
					}
				}
				#ifdef PRINTIT
				printf(" -> SequenceNumber\n");
				#endif
				// Wird nicht gespeichert
				break;
			case 0x51:
				// Tempo
				for (i=0;i<4;i++)
				{
					Text[i]=s[eventpointer];
					#ifdef PRINTIT
					HexPrint(s[eventpointer++],s1); printf("%s|",s1);
					#endif
				}
				#ifdef PRINTIT
				printf(" -> Tempo\n");
				#endif
				prenote=lastnote;
				lastnote=malloc(sizeof(NOTE));
				if(notestart[ActTrack]==NULL) notestart[ActTrack]=lastnote;
				noteend[ActTrack]=lastnote;
				lastnote->mode=1;
				lastnote->Event=event2;
				lastnote->Starttime=ActTime;
				lastnote->Endtime=0;
				for(i=0;i<4;i++)
					lastnote->EventData[i]=Text[i];
				lastnote->Len=4;
				lastnote->Previous=prenote;
				lastnote->Next=NULL;
				if (prenote!=NULL)
					prenote->Next=lastnote;
				
				break;
			case 0x54:
				// SMPTE offset
				for (i=0;i<6;i++)
				{
					#ifdef PRINTIT
					HexPrint(s[eventpointer++],s1); printf("%s|",s1);
					#endif
				}
				#ifdef PRINTIT
				printf(" -> SMPTEOffset\n");
				#endif
				// Wird nicht gespeichert
				break;
			case 0x58:
				// Time-Signature
				len1=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(len1,s1); printf("%s|",s1);
				#endif
				for (i=0;i<len1;i++)
				{
					#ifdef PRINTIT
					HexPrint(s[eventpointer],s1); printf("%s|",s1);
					#endif
					Text[i]=s[eventpointer++];
				}
				#ifdef PRINTIT
				printf(" -> TimeSIgnature\n");
				#endif
				prenote=lastnote;
				lastnote=malloc(sizeof(NOTE));
				if(notestart[ActTrack]==NULL) notestart[ActTrack]=lastnote;
				noteend[ActTrack]=lastnote;
				lastnote->mode=1;
				lastnote->Event=event2;
				lastnote->Starttime=ActTime;
				lastnote->Endtime=0;
				for(i=0;i<len1;i++)
					lastnote->EventData[i]=Text[i];
				lastnote->Len=len1;
				lastnote->Previous=prenote;
				lastnote->Next=NULL;
				if (prenote!=NULL)
					prenote->Next=lastnote;
				break;
			case 0x59:
				// Key-Signature
				len1=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(len1,s1); printf("%s|",s1);
				#endif
				for (i=0;i<len1;i++)
				{
					#ifdef PRINTIT
					HexPrint(s[eventpointer],s1); printf("%s|",s1);
					#endif
					Text[i]=s[eventpointer++];
				}
				#ifdef PRINTIT
				printf(" -> KeySignature\n");
				#endif
				prenote=lastnote;
				lastnote=malloc(sizeof(NOTE));
				if(notestart[ActTrack]==NULL) notestart[ActTrack]=lastnote;
				noteend[ActTrack]=lastnote;
				lastnote->mode=1;
				lastnote->Event=event2;
				lastnote->Starttime=ActTime;
				lastnote->Endtime=0;
				for(i=0;i<len1;i++)
					lastnote->EventData[i]=Text[i];
				lastnote->Len=len1;
				lastnote->Previous=prenote;
				lastnote->Next=NULL;
				if (prenote!=NULL)
					prenote->Next=lastnote;
				break;
			case 0x2F:
				// End of Track
				#ifdef PRINTIT
				HexPrint(s[eventpointer++],s1); printf("%s|",s1);
				printf(" -> TrackEnd\n");
				#endif
				prenote=lastnote;
				lastnote=malloc(sizeof(NOTE));
				if(notestart[ActTrack]==NULL) notestart[ActTrack]=lastnote;
				noteend[ActTrack]=lastnote;
				lastnote->mode=1;
				lastnote->Event=event2;
				lastnote->Starttime=ActTime;
				lastnote->Endtime=0;
				lastnote->Len=0;
				lastnote->Previous=prenote;
				lastnote->Next=NULL;
				if (prenote!=NULL)
					prenote->Next=lastnote;
				
				break;
			case 0x20:
				// Channel Prefix
				for (i=0;i<2;i++)
				{
					#ifdef PRINTIT
					HexPrint(s[eventpointer++],s1); printf("%s|",s1);
					#endif
				}
				#ifdef PRINTIT
				printf(" -> ChannelPrefix\n");
				#endif
				// Wird nicht gespeichert
				break;
			case 0x21:
				// MIDI Port
				for (i=0;i<2;i++)
				{
					#ifdef PRINTIT
					HexPrint(s[eventpointer++],s1); printf("%s|",s1);
					#endif
				}
				#ifdef PRINTIT
				printf(" -> MidiPort\n");
				#endif
				// Wird nicht gespeichert
				break;
			case 0x4B:
				// M-Live-Tag
				len1=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(s[eventpointer++],s1); printf("%s|",s1);
				#endif
				for (i=0;i<len1;i++)
				{
					Text[i]=s[eventpointer];
					#ifdef PRINTIT
					HexPrint(s[eventpointer++],s1); printf("%s|",s1);
					#endif
				}
				Text[++i]=0;
				#ifdef PRINTIT
				printf(" -> M-Live %s\n",Text);
				#endif
				// Wird nicht gespeichert
				break;
		}
	}
	else
	{
		// Midi-Event
		MidiType=event1&0xF0;
		MidiChannel=event1&0x0F;
		// printf("MIDIEVENT xx %d %d xx ",MidiType,MidiChannel);
		switch(MidiType)
		{
			case 0x80:
				// Note Off
				Note=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(Note,s1); printf("%s|",s1);
				#endif
				Velocity=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(Velocity,s1); printf("%s|",s1);
				printf(" -> Note Off %d %d\n",Note,Velocity);
				#endif
				actnote=SearchNote(Note,ActTrack);
				if (actnote!=NULL)
				{
					actnote->Endtime=ActTime;
				}
				break;
			case 0x90:
				// Note On
				Note=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(Note,s1); printf("%s|",s1);
				#endif

				Velocity=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(Velocity,s1); printf("%s|",s1);
				
				printf(" -> NoteOn %d %d\n",Note,Velocity);
				#endif
				prenote=lastnote;
				lastnote=malloc(sizeof(NOTE));
				Notes[ActTrack]++;
				if(notestart[ActTrack]==NULL) notestart[ActTrack]=lastnote;
				noteend[ActTrack]=lastnote;
				lastnote->mode=0;
				lastnote->Event=event1;
				lastnote->Note=Note;
				lastnote->MonoTrack=0;
				lastnote->Velocity=Velocity;
				lastnote->Starttime=ActTime;
				lastnote->Endtime=0;
				lastnote->Previous=prenote;
				lastnote->Next=NULL;
				if (prenote!=NULL)
					prenote->Next=lastnote;
				break;
			case 0xA0:
				// Key-Aftertouch
				Note=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(Note,s1); printf("%s|",s1);
				#endif

				Velocity=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(Velocity,s1); printf("%s|",s1);
				

				printf(" -> Aftertouch %d %d\n",Note,Velocity);
				#endif
				break;
			case 0xB0:
				// ControlChange
				Controller=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(Controller,s1); printf("%s|",s1);
				#endif

				Value=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(Value,s1); printf("%s|",s1);

				printf(" -> ControllerChange %d %d\n",Controller,Value);
				#endif
				break;
			case 0xC0:
				// ProgramChange
				Program=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(Program,s1); printf("%s|",s1);
				
				printf(" -> ProgramChange %d\n",Program);
				#endif
				break;
			case 0xD0:
				// Channel Aftertouch
				AfterTouch=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(AfterTouch,s1); printf("%s|",s1);

				printf(" -> MonoAftertouch %d\n",AfterTouch);
				#endif
				break;
			case 0xE0:
				// PitchWheel
				PitchSmall=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(PitchSmall,s1); printf("%s|",s1);
				#endif

				PitchBig=(uint8_t)s[eventpointer++];
				#ifdef PRINTIT
				HexPrint(PitchBig,s1); printf("%s|",s1);

				printf(" -> PitchWheel %d %d\n",PitchSmall,PitchBig);
				#endif
				break;
		}
		
	}
}

void PrintNotes(int ActTrack)
{
	char s1[100];
	char data[300];
	int i;
	struct note *snote;
	snote=notestart[ActTrack];
	do
	{
		switch(snote->mode)
		{
			case 0:
				#ifdef PRINTIT
				printf("NOTE %d STARTTIME %ld ENDTIME %d Track %d\n",snote->Note,snote->Starttime,snote->Endtime,snote->MonoTrack);
				#endif
				break;
			case 1:
				HexPrint(snote->Event,s1);
				#ifdef PRINTIT
				printf("EVENT %s %ld ",s1,snote->Starttime);
				#endif
				for (i=0;i<snote->Len;i++)
				{
					HexPrint(snote->EventData[i],s1);
					#ifdef PRINTIT
					printf("%s|",s1);
					#endif
				}
				#ifdef PRINTIT
				printf("\n");
				
				#endif
				break;
		}
		snote=snote->Next;
	} while (snote!=NULL);
}

void main(int argc,char *argv[])
{

  	uint16_t i,j,k;
  	uint32_t length;
  	char s1[100],s2[100];
  	char fname1[20],fname2[20];
  	FILE *file1,*file2;
  	int flag;
    
    if (argc>1)
    {
		if(strcmp(argv[1],"-h")==0)
		{
			printf("Usage: midisplit filename.mid outname\n");
			printf("Splits a midifile containing midi-tracks into multiple midi-tracks,\n");
			printf("that are monophonic. Thy can be used to feed monophonic synthesizers,\n");
			printf("like hardware analog-synths.\n");
			printf("Parameter 1 selects the midi-file to be imported.\n");
			printf("Parameter 2 is the name-start of the created output-files.\n\n");
			printf("Example:\n");
			printf("midisplit filename.mid ABC\n");
			printf("Splits the midifile with name filename.mid.\n");
			printf("Output-files start with letters ABC.\n\n");
			printf("-h as first parameter shows this help.\n\n");
			exit(1);
		}
  	}
  	if (argc<=2)
  	{
    	printf("Error! To less parameters\n");
			printf("Usage: midisplit filename.mid outname\n");
			printf("Splits a midifile containing midi-tracks into multiple midi-tracks,\n");
			printf("that are monophonic. Thy can be used to feed monophonic synthesizers,\n");
			printf("like hardware analog-synths.\n");
			printf("Parameter 1 selects the midi-file to be imported.\n");
			printf("Parameter 2 is the name-start of the created output-files.\n\n");
			printf("Example:\n");
			printf("midisplit filename.mid ABC\n");
			printf("Splits the midifile with name filename.mid.\n");
			printf("Output-files start with letters ABC.\n\n");
			printf("-h as first parameter shows this help.\n\n");
    	exit(1);
	}
	strcpy(fname1,argv[1]);
	strcpy(fname2,argv[2]);
	file1=fopen(fname1,"rb");
	if ((file1==NULL)||(file2==NULL))
	{
    	printf("Error! File cant be opened\n");
			printf("Usage: midisplit filename.mid outname\n");
			printf("Splits a midifile containing midi-tracks into multiple midi-tracks,\n");
			printf("that are monophonic. Thy can be used to feed monophonic synthesizers,\n");
			printf("like hardware analog-synths.\n");
			printf("Parameter 1 selects the midi-file to be imported.\n");
			printf("Parameter 2 is the name-start of the created output-files.\n\n");
			printf("Example:\n");
			printf("midisplit filename.mid ABC\n");
			printf("Splits the midifile with name filename.mid.\n");
			printf("Output-files start with letters ABC.\n\n");
			printf("-h as first parameter shows this help.\n\n");
    	exit(1);
  	}
  	printf("Read of midi-file and Track %s\n",argv[1]);
  	if(ReadHeader(file1))
  	{
		#ifdef PRINTIT
    	printf("INIT %s HL %d HF %d NT %d DV %d\n",s,header_length,header_format,header_nt,header_division);
		#endif
		#ifdef STOPIT
    	scanf("%s",s1);
		#endif
  	}
  	else
  	{
    	printf("Error!\n");
		exit(1);
  	}
  	for (i=0;i<header_nt;i++)
  	{
		notestart[i]=NULL;
		noteend[i]=NULL;
		lastnote=NULL;
		ActTime=0;
		#ifdef STOPIT
 		scanf("%s",s1);
		#endif
 		if (ReadTrackStart(file1))
 		{
 			Tracks[i]=malloc(track_length);
 			Lens[i]=track_length;
 			printf("Original-Track %d has length of %ld bytes\n",i+1,track_length);
			#ifdef STOPIT
 			scanf("%s",s1);
			#endif
 			if (fread(Tracks[i],track_length,1,file1)==1)
 			{
 				j=0;
 				eventpointer=0;
 				while(eventpointer<Lens[i])
 				{
 					OutputTrackInfo(Tracks[i],i);
					#ifdef STOPIT
					scanf("%s",s1);
					#endif
 				}
 			}
 			else
 			{
 				printf("Error while reading trackdata of Track %d\n",i+1);
 				exit(1);
 			}
 		}   
 		else
 		{
 			printf("Error while reading track %d!\n",i+1);
 			exit(1);
 		}
  	}
  	fclose(file1);
	
	for (i=0;i<header_nt;i++)
	{
		printf("######################################");
		printf("\nSplitting track %d - ",i+1);
		SplitPolyphony(i);
		printf("has %d notes, MaxPolyphony %d\n",Notes[i],MaxPolyphony[i]);
		PrintNotes(i);
		printf("Output of splitted tracks\n");
		for (j=0;j<MaxPolyphony[i];j++)
		{
			printf("Original-track %d mono-track %d\n",i+1,j+1);
			sprintf(s1,"%s-OT-%3d-MT-%3d.mid",fname2,i,j+1);
			for(k=0;k<strlen(s1);k++)
			{
				if (s1[k]==' ') s1[k]='0';
			}
			length=WriteMonoTrack(file1,j,false,i);		
			printf("%s has %ld Bytes ... ",s1,length);
		
			file1=fopen(s1,"wb");
		
			// Header schreiben
			strcpy(s1,"MThd");
			fwrite(s1,1,4,file1);
			Write32BE(file1,header_length);
			Write16BE(file1,header_format);
			Write16BE(file1,1);
			Write16BE(file1,header_division);
			// Track-Init schreiben
			strcpy(s1,"MTrk");
			fwrite(s1,1,4,file1);
			Write32BE(file1,length);		
			length=WriteMonoTrack(file1,j,true,i);
			printf("%ld Bytes written\n",length);
			fclose(file1);
		}
	}		
}
