#include "pds/xtc/CDatagram.hh"
#include "pds/service/CmdLineTools.hh"
#include "pds/service/GenericPool.hh"
#include "timetool/event/TimeToolC.hh"
#include "pdsdata/xtc/XtcFileIterator.hh"
#include "pdsdata/xtc/Xtc.hh"
#include "pdsdata/xtc/DetInfo.hh"
#include "pdsdata/xtc/XtcIterator.hh"
#include "pdsdata/ana/XtcRun.hh"

#include <new>
#include <limits.h>
#include <glob.h>

using namespace Pds;

static bool debug = false;

static const unsigned MAX_DG_SIZE = 0x3000000;

void usage(char* progname) {
  fprintf(stderr,
          "Usage: %s -p <path> -r <run> -i <id>\n",
          progname);
}

class OWire {
public:
  OWire() : _f(0) {}
public:
  void open(const char* oname) {
    _f = fopen(oname,"w");
    if (_f)
      fprintf(_f,"%9.9s  %9.9s  %12.12s  %12.12s  %12.12s  %12.12s\n",
              "seconds","nseconds","position","amplitude","ref_ampl","nxt_ampl");
  }
  void close() {
    if (_f) fclose(_f);
    _f = 0;
  }
  void event(InDatagram* dg, const ::TimeTool::Fex* fex) {
    if (_f) {
      double position = fex ? fex->filtered_position() : 0.0;
      double amplitude = fex ? fex->amplitude() : 0.0;
      double ref_ampl = fex ? fex->ref_amplitude() : 0.0;
      double nxt_ampl = fex ? fex->next_amplitude() : 0.0;

      fprintf(_f,
              "%09d  %09d  %12f  %12f  %12f  %12f\n",
              dg->datagram().seq.clock().seconds(),
              dg->datagram().seq.clock().nanoseconds(),
              position,
              amplitude,
              ref_ampl,
              nxt_ampl);
    }
  }
private:
  FILE* _f;
};

int main(int argc, char* argv[]) {
  int c;
  bool verbose = false;
  bool write_ref = false;
  const char* fname = 0;
  const char* oname = 0;
  const char* path = 0;
  const char* refpath = 0;
  unsigned run = 0;
  unsigned parseErr = 0;
  unsigned events = -1U;
  DetInfo detInfo(0, Pds::DetInfo::NumDetector, 0, DetInfo::Opal1000, 0);
  char dirpath[PATH_MAX];
  
  while ((c = getopt(argc, argv, "dhvwn:o:p:r:i:f:R:")) != -1) {
    switch (c) {
    case 'd': debug = true; break;
    case 'h':
      usage(argv[0]);
      exit(0);
    case 'i':
      if (!CmdLineTools::parseDetInfo(optarg,detInfo)) {
        printf("%s: option `-i' parsing error\n", argv[0]);
        parseErr++;
      }
      break;
    case 'f':
      fname = optarg;
      break;
    case 'o':
      oname = optarg;
      break;
    case 'n':
      events = atoi(optarg);
      break;
    case 'r':
      run = atoi(optarg);
      break;
    case 'p':
      path = optarg;
      break;
    case 'v':
      verbose = true;
      break;
    case 'w':
      write_ref = true;
      break;
    case 'R':
      refpath = optarg;
      break;
    default:
      parseErr++;
    }
  }
  
  if (!(parseErr==0 && run && path && (detInfo.detector() != Pds::DetInfo::NumDetector))) {
    usage(argv[0]);
    exit(2);
  }

  if (!refpath) {
    if(getcwd(dirpath, PATH_MAX)) {
      refpath = dirpath;
    }
  }

  Pds::Ana::XtcRun files;

  char buff[256];
  sprintf(buff,"%s/*-r%04d-s*.xtc",path,run);
  glob_t g;
  glob(buff, 0, 0, &g);

  if (debug)
    printf("glob %s found %zd files\n",buff,g.gl_pathc);

  for(unsigned i=0; i<g.gl_pathc; i++) {
    char* p = g.gl_pathv[i];
    if (debug) printf("adding file %s\n",p);
    if (i==0) files.reset   (p);
    else      files.add_file(p);
  }

  files.init();

  Dgram* dg = NULL;
  int slice = -1;
  int64_t offset = -1;
  unsigned ievent = 0;

  Pds_TimeTool_event::TimeToolC app = fname ?
    Pds_TimeTool_event::TimeToolC(fname, write_ref, verbose, refpath) :
    Pds_TimeTool_event::TimeToolC(detInfo, write_ref, verbose, refpath);
  OWire owire;
  if (oname) owire.open(oname);

  GenericPool pool(MAX_DG_SIZE,1);

  while(1) {
    Pds::Ana::Result result = files.next(dg, &slice, &offset);
    if (result == Pds::Ana::OK) {

      if (debug)
        printf("%d.%09d %s\n",
               dg->seq.clock().seconds(),
               dg->seq.clock().nanoseconds(),
               TransitionId::name(dg->seq.service()));

      CDatagram* cdg = new(&pool) CDatagram(*dg,dg->xtc);

      switch (dg->seq.service()) {
      case Pds::TransitionId::L1Accept:
        app.events(cdg);
        owire.event(cdg,app.fex());
        ievent++;
        break;
      default:
        { Transition tr(dg->seq.service(),Transition::Execute,dg->seq,dg->env);
          app.transitions(&tr);
          app.events(cdg);
        } break;
      }

      delete cdg;
    }
    else break;

    if (ievent >= events) break;
    if (ievent && (ievent%1000)==0)
      printf("event %d\n",ievent);
  }

  owire.close();

  return 0;
}

