/* -*- c++ -*- */
/* 
 * Copyright 2014 <+YOU OR YOUR COMPANY+>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "sync_and_channel_estimaton_impl.h"
#include <complex>
#include <stdio.h>
#include <gnuradio/expj.h>
#include <gnuradio/math.h>

namespace gr {
  namespace isdbt {

  // The carriers positions
  const int tmcc_carriers[52] = {
		  70, 	133, 233, 410, \
		  476, 	587, 697, 787, \
		  947, 	1033, 1165, 1289,\
		  1319, 1474, 1537, 1637,\
		  1814, 1880, 1991, 2101,\
		  2191, 2351, 2437, 2569,\
		  2693, 2723, 2878, 2941,\
		  3041, 3218, 3284, 3395,\
		  3505, 3595, 3755, 3841,\
		  3973, 4097, 4127, 4282,\
		  4345, 4445, 4622, 4688,\
		  4799, 4909, 4999, 5159,\
		  5245, 5377, 5501, 5531
  };

  int previous_symbol;

    sync_and_channel_estimaton::sptr
    sync_and_channel_estimaton::make()
    {
      return gnuradio::get_initial_sptr
        (new sync_and_channel_estimaton_impl());
    }

    /*
     * Esta función es robada de  la implementación de dvb-t y sirve para la sincronización realizada en la parte anterior a la FFT
     * La copié y pegué tal cual
     * ---------------------------------------------------------------------
     */
    int
	sync_and_channel_estimaton_impl::is_sync_start(int nitems)
    {
      std::vector<tag_t> tags;
      const uint64_t nread = this->nitems_read(0); //number of items read on port 0
      this->get_tags_in_range(tags, 0, nread, nread + nitems, pmt::string_to_symbol("sync_start"));

      return tags.size() ? 1 : 0;
    }
    /*
     * ---------------------------------------------------------------------
     */
    gr_complex
	sync_and_channel_estimaton_impl::get_pilot_value(int index)
    {
      //TODO - can be calculated at the beginning
    	/* Obtenemos el valor +4/3 cuando d_wk[tmcc] = 0 y -4/3 cuando d_wk[tmcc] = 1*/
      return gr_complex((float)(4 * 2 * (0.5 - d_wk[index])) / 3, 0);
    }
    /*
     * ---------------------------------------------------------------------
     */
    void
	sync_and_channel_estimaton_impl::generate_prbs() // En dvb-t el prbs es idéntico que en isdb-t
    {
      // init PRBS register with 1s
      unsigned int reg_prbs = (1 << 11) - 1; // Inicializamos reg_prbs en 111111111111

      for (int k = 0; k < (active_carriers); k++)
      {
        d_wk[k] = (char)(reg_prbs & 0x01); // Sale el bit menos significativo en forma de caracter
        int new_bit = ((reg_prbs >> 2) ^ (reg_prbs >> 0)) & 0x01; // Este es el bit que entra a la izquierda del registro
        reg_prbs = (reg_prbs >> 1) | (new_bit << 10); // Movemos todo el registro para la derecha y agregamos el new_bit a la izquierda.

        //printf("prbs[%d] = %d\n",k,d_wk[k]);
      }
    }
    /*
     * ---------------------------------------------------------------------
     */

    /*
     * The private constructor
     * Este constructor lo creamos basado en la implementación de dvb-t que ya tenemos. Como la condiguración del transmisor en esta primera instancia la vamos a tener como fija,
     * no pasamos ningún parámetro.
     */
    sync_and_channel_estimaton_impl::sync_and_channel_estimaton_impl()
      : gr::block("sync_and_channel_estimaton",
              gr::io_signature::make(1, 1, sizeof(gr_complex)*8192),
			  //gr::io_signature::make(1, 1, sizeof(gr_complex)*52))
			  gr::io_signature::make(1, 1, sizeof(gr_complex)*5617))

    /*
     * La relación entre salidas y entradas en realidad la marcamos acá. Por cada de entrada de 8192 complejos tenemos una salida de 4992 complejos. La tasa es 1:1, pero en realidad
     * estamos haciendo cierto decimation.
     * Para probar sólo sacar las carrier continuas, pongo la salida en 45 y la entrada en 2048, pero para isdbt tienen que ser los valores que dice arriba.
     */
    {
        active_carriers = 5617;
    	/*
    	 * Definimos el tamaño de la entrada y la salida
    	 */
        //d_ninput = ninput;
        //d_noutput = noutput;

    	d_ninput = 8192;
    	//d_noutput =52;
    	d_noutput =5617;


        //FFT length
        d_fft_length = 8192;


        // Number of zeros on the left of the IFFT
        d_zeros_on_left = int(ceil((d_fft_length - (active_carriers)) / 2.0));

        // Max frecuency offset to be corrected
        d_freq_offset_max = 200;

        // Number of tmcc pilots
        tmcc_carriers_size=52;

        // Number of sp pilots
        sp_carriers_size=5616/12;

        // Generate PRBS
    	generate_prbs();

        // Obtain phase diff for all tmcc pilots
        for (int i = 0; i < (tmcc_carriers_size - 1); i++)
        {
          d_known_phase_diff[i] = \
            std::norm(get_pilot_value(tmcc_carriers[i + 1]) - get_pilot_value(tmcc_carriers[i]));
        }
        
        // Allocate buffer for deroated input symbol
        derotated_in = new gr_complex[d_fft_length];
        if (derotated_in == NULL)
        {
          std::cout << "error allocating derotated_in" << std::endl;
          return;
        }
    }

    /*
     * Our virtual destructor.
     */
    sync_and_channel_estimaton_impl::~sync_and_channel_estimaton_impl()
    {
    }

    void
    sync_and_channel_estimaton_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
    	/*
    	 * Default implementation is:
    	 *
    	 *	unsigned ninputs = ninput_items_required.size ();
    	 *	for(unsigned i = 0; i < ninputs; i++)
      	 *	ninput_items_required[i] = noutput_items;
    	 */
        int ninputs = ninput_items_required.size();

        for (int i = 0; i < ninputs; i++)
          ninput_items_required[i] =  2* noutput_items;
        /*
         * Por lo que yo entendí esta relación tiene más que ver con el history que con otra cosa. Seguimos teniendo una relación 1:1 entre salidas y entradas
         */
    }

    /*
     * process_tmcc_data function
     * post-fft frequency offset estimation
     * -----------------------------------------------------------------------------------------
     */

    void
	sync_and_channel_estimaton_impl::process_tmcc_data(const gr_complex * in)
    {
      // Look for maximum correlation for tmccs
      // in order to obtain postFFT integer frequency correction

    float max = 0; 
    gr_complex sum = 0;
    int start = 0;
    gr_complex phase;

      for (int i = d_zeros_on_left - d_freq_offset_max; i < d_zeros_on_left + d_freq_offset_max; i++)
      {
        sum = 0;
        for (int j = 0; j < (tmcc_carriers_size - 1); j++)
        {

        	if (d_known_phase_diff[j] == 0)
        		phase = in[i + tmcc_carriers[j + 1]]*conj(in[i + tmcc_carriers[j]]);//norm devuelve la norma del complejo, e.g. norm(3,4) = 25
        	else
        		phase = -in[i + tmcc_carriers[j + 1]]*conj(in[i + tmcc_carriers[j]]);
            //sum +=d_known_phase_diff[2]*phase;
            sum +=phase;

            /* ORIGINALMENTE ERA DE ESTA MANERA
              phase = std::norm(in[i + tmcc_carriers[j + 1]] - in[i + tmcc_carriers[j]]);
              sum += d_known_phase_diff[j] * phase;

          /* d_known_phase_diff[j] es el valor que debería dar phase en un caso ideal */
        }
       // printf("sum=%f\n",sum);
        if (abs(sum) > max)
        {
        /*
         * Cuando sum es maximo es que en i tenemos la PRIMER piloto, i.e. el ubicado en cero
         */
          max = abs(sum);
          start = i;
        }
        //printf("max=%f\n",max);
      }

      d_freq_offset = start - d_zeros_on_left;
      /*
       * En d_freq_offset se guarda la cantidad ENTERA de muestras que tenemos de desfasaje frecuencial
       */
     // printf("max: %f\n", max);
      //printf("start: %i\n", start);
      //if (d_freq_offset)
        //printf("d_freq_offset: %i\n", d_freq_offset);
    }


    gr_complex *
	sync_and_channel_estimaton_impl::frequency_correction(const gr_complex * in, gr_complex * out)
    {
      for (int k = 0; k < d_fft_length; k++)
      {
        out[k] = in[k + d_freq_offset];
      }

      return (out);
    }

    /*
     * process_sp_data function
     * post-fft frequency offset estimation
     * -----------------------------------------------------------------------------------------
     */

    void
	sync_and_channel_estimaton_impl::process_sp_data(const gr_complex * in)
    {
        /* First thing is to locate the scattered pilots
        */

        /*************************************************************/
        // Find out the OFDM symbol index (value 0 to 3) sent
        // in current block by correlating scattered symbols with
        // current block - result is (symbol index % 4)
        /*************************************************************/
        float max = 0; 
        gr_complex sum;
        int current_symbol = 0; 


        int next_sp_carrier; 
        int current_sp_carrier; 
        gr_complex phase; 
        for (int sym_count = 0; sym_count < 4; sym_count++)
        {
            sum = 0; 
            for (int i=0; i < sp_carriers_size-1; i++)
            {
                next_sp_carrier = 12*(i+1)+3*sym_count; 
                current_sp_carrier = 12*i+3*sym_count; 

                if (d_wk[next_sp_carrier]==d_wk[current_sp_carrier])
                    phase = in[next_sp_carrier+d_zeros_on_left]*conj(in[current_sp_carrier+d_zeros_on_left]);
                else
                    phase = -in[next_sp_carrier+d_zeros_on_left]*conj(in[current_sp_carrier+d_zeros_on_left]);
                sum += phase; 
                //printf("next_sp_carrier: %i; current_sp_carrier: %i\n", next_sp_carrier, current_sp_carrier); 
            }
            //printf("sum: %f; sym_count: %i\n", abs(sum), sym_count); 
            if (abs(sum)>max)
            {
                max = abs(sum); 
                current_symbol = sym_count; 
            }
        }
        //if (((current_symbol-previous_symbol) !=1) && ((current_symbol-previous_symbol) !=-3)) printf("previous_symbol: %i, \n current_symbol: %i\n", previous_symbol, current_symbol);
        previous_symbol = current_symbol;


        /*************************************************************/
        // Keep data for channel estimator
        // This method interpolates scattered measurements across one OFDM symbol
        // It does not use measurements from the previous OFDM symnbols (does not use history)
        // as it may have encountered a phase change for the current phase only
        /*************************************************************/

        // I first calculate the channel gain on the SP carriers. 
        for (int i = 0; i < sp_carriers_size; i++)
        {
            current_sp_carrier = 12*i+3*current_symbol; 
            d_channel_gain[current_sp_carrier] = in[current_sp_carrier+d_zeros_on_left]/get_pilot_value(current_sp_carrier); 
        }

        // I then interpolate to obtain the channel gain on the rest of the carriers. 
        gr_complex tg_alpha; 
        for (int i = 0; i < sp_carriers_size-1; i++)
        {
            current_sp_carrier = 12*i+3*current_symbol; 
            next_sp_carrier = 12*(i+1)+3*current_symbol; 
            // Calculate tg(alpha) due to linear interpolation
            tg_alpha = (d_channel_gain[next_sp_carrier] - d_channel_gain[current_sp_carrier]) / gr_complex(12.0, 0.0);


            // Calculate interpolation for all intermediate values
            for (int j = 1; j < next_sp_carrier-current_sp_carrier; j++)
            {
                d_channel_gain[current_sp_carrier+j] = d_channel_gain[current_sp_carrier] + tg_alpha * gr_complex(j, 0.0);
            }

        }

    }

    /*
     *
     * general_work function
     * Here is where all the signal processing is done
     * ----------------------------------------------------------------------------------------
     */

    int
    sync_and_channel_estimaton_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {

        const gr_complex *in = (const gr_complex *) input_items[0];
        gr_complex *out = (gr_complex *) output_items[0];

        /*
         * Here starts the processing
         */
        // Process tmcc data
        for (int i = 0; i < noutput_items; i++)
        {
        	process_tmcc_data(&in[i* d_ninput]);

        	// Correct ofdm symbol
        	frequency_correction(&in[i* d_ninput], derotated_in);
            process_sp_data(&derotated_in[i*d_ninput]); 
            
            // TODO should this go outside the loop??

            for (int carrier = 0; carrier < active_carriers; carrier++)
            //for (int carrier = 0; carrier < 52; carrier++)
			{

            	//out[i*d_noutput +carrier] = derotated_in[d_zeros_on_left]/d_channel_gain[0];
            	//out[i*d_noutput +carrier] = derotated_in[tmcc_carriers[carrier]+d_zeros_on_left]/d_channel_gain[tmcc_carriers[carrier]];
            	//out[i*d_noutput +carrier] = derotated_in[70+d_zeros_on_left]/d_channel_gain[70];
        	    out[i*d_noutput +carrier] = derotated_in[carrier+d_zeros_on_left]/d_channel_gain[carrier];
        	    //printf("tmcc_carriers[%d]=%d\n",carrier,tmcc_carriers[carrier]);
        	    //printf("carrier =%d\n",carrier);
            }
        }

    
        //printf("-->>d_freq_offset: %i\n", d_freq_offset);



        /*
         * Acá finaliza el procesamiento
         */


        // Tell runtime system how many input items we consumed on
        // each input stream.
        consume_each (noutput_items);

        // Tell runtime system how many output items we produced.
        return noutput_items;
    }

  } /* namespace isdbt */
} /* namespace gr */

