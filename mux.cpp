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

// Callback function that simply prints the information to cout
int print_callback(FANN::neural_net &net, FANN::training_data &train,
    unsigned int max_epochs, unsigned int epochs_between_reports,
    float desired_error, unsigned int epochs, void *user_data)
{
    cout << "Epochs     " << setw(8) << epochs << ". "
         << "Current Error: " << left << net.get_MSE() << right << endl;
    return 0;
}

jack_port_t *input_port;
jack_client_t *client;

int ze_mode = 0;

unsigned int enough_positive = 44100;
unsigned int enough_negative = 44100*5;
jack_default_audio_sample_t *samples_of_a = NULL;
jack_default_audio_sample_t *samples_of_not_a = NULL;

int process (jack_nframes_t nframes, void *arg) {
  jack_default_audio_sample_t *in;
  static unsigned int posish = 0;
  
  if ( ze_mode == 1 ) {
    // capture 1 second of positive audio (A!!)
    if(posish < enough_positive ) {
      in = (jack_default_audio_sample_t *)jack_port_get_buffer (input_port, nframes);
      memcpy (samples_of_a+posish, in,
        sizeof (jack_default_audio_sample_t) * nframes);
      posish += nframes;
    } else {
      posish = 0;
      ze_mode = 0;
    }
  } else if ( ze_mode == 2 ) {
    // capture 1 second of negative audio (not A!!)
    if( posish < enough_negative ) {
      in = (jack_default_audio_sample_t *)jack_port_get_buffer (input_port, nframes);
      memcpy (samples_of_not_a+posish, in,
        sizeof (jack_default_audio_sample_t) * nframes);
      posish += nframes;
    } else {
      posish = 0;
      ze_mode = 0;
    }
  } else return -1;

  return 0;      
}

void jack_shutdown (void *arg) { 
  delete[] samples_of_a;
  delete[] samples_of_not_a;
  fprintf (stderr, "boozah");
  exit (1);
}

int main ( int argc, char *argv[] ) {
  const char **ports;
  const char *client_name = "mux";
  const char *server_name = NULL;
  jack_options_t options = JackNullOption;
  jack_status_t status;
  samples_of_a = new jack_default_audio_sample_t[enough_positive+1024];
  samples_of_not_a = new jack_default_audio_sample_t[enough_negative+1024];
  
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

  std::cout << ">8( A    when you're ready ... " << std::endl;
  std::cin >> ze_mode;
  ze_mode = 1;
  sleep (3);

  std::cout << ">8( Not A   when you're ready ... " << std::endl;
  std::cin >> ze_mode;
  ze_mode = 2;
  sleep (7);


  ////////////////////////////
  const float learning_rate = 0.7f;
  const unsigned int num_layers = 3;
  const unsigned int num_input = 512;
  const unsigned int num_hidden = 512;
  const unsigned int num_output = 1;
  const float desired_error = 0.05f;
  const unsigned int max_iterations = 1000;
  const unsigned int iterations_between_reports = 4;
  
  unsigned int num_examples = 128;
  
  cout << endl << "Creating network." << endl;
  
  FANN::neural_net net;
  net.create_standard(num_layers, num_input, num_hidden, num_output);
  
  net.set_learning_rate(learning_rate);
  
  net.set_activation_steepness_hidden(1.0);
  net.set_activation_steepness_output(1.0);
  
  net.set_activation_function_hidden(FANN::SIGMOID_SYMMETRIC_STEPWISE);
  net.set_activation_function_output(FANN::SIGMOID_SYMMETRIC_STEPWISE);
  
  // Set additional properties such as the training algorithm
  //net.set_training_algorithm(FANN::TRAIN_QUICKPROP);
  
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
  
  cout << endl << "Training network." << endl;
  
  FANN::training_data data;
  
  fann_type ** training_input = new fann_type *[num_examples];
  fann_type ** training_output = new fann_type *[num_examples];
  
  // allocate arrays
  for( int ex = 0; ex < num_examples; ++ex) {
    training_input[ex]  = new fann_type[num_input];
    training_output[ex] = new fann_type[num_output];
  }
  
  // fill with audio data
  for( int ex = 0; ex < num_examples; ++ex) {
    bool is_positive = ((rand()%6) == 0);
    int sizey = is_positive ? enough_positive : enough_negative;
    int rndspot = rand()%(sizey - num_input - 1);
    for( int i = 0; i < num_input; ++i ) {
      if( is_positive )
        training_input[ex][i] = (fann_type)(samples_of_a[ rndspot+i ]);
      else
        training_input[ex][i] = (fann_type)(samples_of_not_a[ rndspot+i ]);
      
      training_output[ex][i] = (fann_type)(is_positive ? 1.0 : 0.0 );
    }
  }
  
  data.set_train_data(
      num_examples,
      num_input,
      training_input,
      num_output,
      training_output);
  cout << endl << "!!!!!!!!!!!!!!!Did it!." << endl;
  
  // Initialize and train the network with the data
  net.init_weights(data);
  
  cout << "Max Epochs " << setw(8) << max_iterations << ". "
      << "Desired Error: " << left << desired_error << right << endl;
  net.set_callback(print_callback, NULL);
  net.train_on_data(data, max_iterations, iterations_between_reports, desired_error);
  
  /*cout << endl << "Testing network." << endl;
  
  for (unsigned int i = 0; i < data.length_train_data(); ++i)
  {
      // Run the network on the test data
      fann_type *calc_out = net.run(data.get_input()[i]);
  
      cout << "XOR test (" << showpos << data.get_input()[i][0] << ", " 
           << data.get_input()[i][1] << ") -> " << *calc_out
           << ", should be " << data.get_output()[i][0] << ", "
           << "difference = " << noshowpos
           << fann_abs(*calc_out - data.get_output()[i][0]) << endl;
  }
  */
  
  cout << endl << "Saving network." << endl;
  
  // Save the network in floating point and fixed point
  net.save("mux.net");
  unsigned int decimal_point = net.save_to_fixed("mux.net");
  data.save_train_to_fixed("mux.data", decimal_point);
  
  cout << endl << "Mux test completed." << endl;
  //////////////////


  jack_client_close (client);
  delete[] samples_of_a;
  delete[] samples_of_not_a;
  std::cout<< "boozah" << std::endl;
  exit (0);
}
