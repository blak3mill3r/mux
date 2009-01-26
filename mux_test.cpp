#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <iomanip>

#include <jack/jack.h>
#include <floatfann.h>
#include <fann_cpp.h>

using std::cout;
using std::cerr;
using std::endl;
using std::setw;
using std::left;
using std::right;
using std::showpos;
using std::noshowpos;

jack_port_t *input_port;
jack_client_t *client;
FANN::neural_net net;
fann_type *listen;

jack_default_audio_sample_t *recent_sample = NULL;

int process (jack_nframes_t nframes, void *arg) {
  jack_default_audio_sample_t *in;
  static unsigned int posish = 0;
  if( (++posish) % 16 == 0) {
  
    in = (jack_default_audio_sample_t *)jack_port_get_buffer (input_port, nframes);

    //memcpy (recent_sample, in,
    //  sizeof (jack_default_audio_sample_t) * nframes);
    
    for(int i = 0; i < 512; ++i)
      listen[i] = (fann_type)(in[i]);
      
    fann_type *calc_out = net.run(listen);
    
    cout << "thinking of " << *calc_out << endl;
  }
  return 0;      
}

void jack_shutdown (void *arg) { 
  delete[] recent_sample;
  delete[] listen;
  fprintf (stderr, "boozah out");
  exit (1);
}

int main ( int argc, char *argv[] ) {
  const char **ports;
  const char *client_name = "mux_test";
  const char *server_name = NULL;
  jack_options_t options = JackNullOption;
  jack_status_t status;
  recent_sample = new jack_default_audio_sample_t[1024];
  listen = new fann_type[512];

  cout << ">8( DOING IT ... ";
  
  if(net.create_from_file("/home/blake/w/mux/mux.net"))
    cout << "yay";
  else
    cout << "nay";

  cout << endl << "horseshit network." << endl;

  client = jack_client_open (client_name, options, &status, server_name);

  if (client == NULL) {
    fprintf (stderr, "8'(   jack_client_open() failed, " "status = 0x%2.0x\n", status);
    if (status & JackServerFailed) { fprintf (stderr, "8'(   Unable to connect to JACK server\n"); }
    exit (1);
  }
  if (status & JackServerStarted) {
    fprintf (stderr, "JACK server started\n");
  }
  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(client);
    fprintf (stderr, "unique name `%s' assigned\n", client_name);
  }

  jack_set_process_callback (client, process, 0);

  jack_on_shutdown (client, jack_shutdown, 0);

  printf ("engine sample rate: %u\n", jack_get_sample_rate (client));

  input_port = jack_port_register (client, "input",
           JACK_DEFAULT_AUDIO_TYPE,
           JackPortIsInput, 0);

  if (input_port == NULL) {
    fprintf(stderr, "8'(   no more JACK ports available\n");
    exit (1);
  }

  /* SHOWTIME */
  if (jack_activate (client)) {
    fprintf (stderr, "cannot activate client");
    exit (1);
  }

  // Output network type and parameters
  cout << endl << "Network Type                         :  ";
  switch (net.get_network_type())
  {
  case FANN::LAYER:
      cout << "LAYER" << endl;
      break;
  case FANN::SHORTCUT:
      cout << "SHORTCUT" << endl;
      break;
  default:
      cout << "UNKNOWN" << endl;
      break;
  }
  net.print_parameters();
  
  cout << endl << "Doing it with network." << endl;
  sleep (-1);
  
  cout << endl << "Mux test completed." << endl;
  //////////////////


  jack_client_close (client);
  delete[] recent_sample;
  delete[] listen;
  std::cout<< "boozah" << std::endl;
  exit (0);
}
