#--------------------------------------------------------------------------
# File and Version Information:
#  $Id$
#
# Description:
#  Pyana user analysis module my_ana_mod...
#
#------------------------------------------------------------------------

"""User analysis module for pyana framework.

This software was developed for the LCLS project.  If you use all or 
part of it, please give an appropriate acknowledgment.

@version $Id$

@author Matthew J. Weaver
"""

#------------------------------
#  Module's version from SVN --
#------------------------------
__version__ = "$Revision$"
# $Source$

#--------------------------------
#  Imports of standard modules --
#--------------------------------
import sys
import logging

#-----------------------------
# Imports for other modules --
#-----------------------------
from pypdsdata import xtc
import numpy as np

#----------------------------------
# Local non-exported definitions --
#----------------------------------

# local definitions usually start with _
_BYKICK_CODE = 162

#---------------------
#  Class definition --
#---------------------
class ttref_db:
    def __init__ (self,fname):
        self.m_entries = []
        f = open(fname,"r")
        while(True):
            line = f.readline().split()
            if len(line)==0:
                break
            ts = xtc.ClockTime( int(line[0],16),
                                int(line[1],16) )
            line = f.readline().split()
            sp = []
            for i in range(len(line)):
                sp.append(float(line[i]))
            self.m_entries.append((ts, sp))
        f.close()

    def is_before(self, a, b):
        if a.seconds() < b.seconds():
            return True
        if a.seconds() > b.seconds():
            return False
        if a.nanoseconds() < b.nanoseconds():
            return True
        return False

    def lookup(self, ts):
        for i in range(1,len(self.m_entries)):
            if self.is_before(ts,self.m_entries[i][0]):
                return self.m_entries[i][1]
        return self.m_entries[-1][1]
    
class timetool (object) :
    """Class whose instance will be used as a user analysis module. """

    #--------------------
    #  Class variables --
    #--------------------
    
    # usual convention is to prefix static variables with s_
    s_staticVariable = 0

    #----------------
    #  Constructor --
    #----------------
    def __init__ ( self,
                   det_name='AmoEndstation-0|Opal1000-0',
                   lo_column='100', hi_column='800',
                   sig_lo_row='640', sig_hi_row='680', ref_convergence='1.0',
                   drk_lo_row='540', drk_hi_row='580', drk_convergence='0.05',
                   sbd_lo_row='500', sbd_hi_row='540', sbd_avg_smp=None,
                   calib_p0='0', calib_p1='1', calib_p2='0',
                   laser_eventcode=None,
                   ref_db=None,
                   filter_wts=None,
                   put_spectrum_smp='0',
                   debug=False) :
        """Class constructor. The parameters to the constructor are passed
        from pyana configuration file. If parameters do not have default 
        values  here then the must be defined in pyana.cfg. All parameters 
        are passed as strings, convert to correct type before use.

        @param det_name          TimeTool spectrometer camera
        @param lo_column         Beginning column of valid spectrum
        @param hi_column         Ending column of valid spectrum
        @param sig_lo_row        Signal region lo row on camera
        @param sig_hi_row        Signal region hi row on camera
        @param ref_convergence   Reference averaging (inverse num of events)
        @param sbd_lo_row        Sideband region lo row on camera (Continuum w/o x-rays)
        @param sbd_hi_row        Sideband region hi row on camera
        @param sbd_avg_smp       Columns to average in correction
        @param drk_lo_row        Dark region lo row on camera
        @param drk_hi_row        Dark region hi row on camera
        @param drk_convergence   Dark region averaging (inverse num of events)
        @param calib_p0          Pixel to time(fs) calibration 0th-order constant
        @param calib_p1          Pixel to time(fs) calibration 1st-order constant
        @param calib_p2          Pixel to time(fs) calibration 2nd-order constant
        @param laser_eventcode   Eventcode indicating continuum laser (negative value indicates absence)
        @param ref_db            File of timestamped reference spectra
        @param filter_wts        Matched filter weights (array)
        @param put_spectrum_smp  Put # samples of computed spectrum into event
        @param debug             Print debug info
        """

        # define instance variables, usual conventionis to prefix 
        # these variables with m_ (for "member")
        self.m_det_name = det_name
        self.m_lo_column = int(lo_column)
        self.m_hi_column = int(hi_column)
        self.m_sig_lo_row = int(sig_lo_row)
        self.m_sig_hi_row = int(sig_hi_row)
        self.m_ref_convergence = float(ref_convergence)
        self.m_drk_lo_row = int(drk_lo_row)
        self.m_drk_hi_row = int(drk_hi_row)
        self.m_drk_convergence = float(drk_convergence)
        self.m_drk_even = 0.
        self.m_drk_odd  = 0.
        
        if sbd_avg_smp==None:
            self.m_use_sbd = False
        else:
            self.m_use_sbd = True
            self.m_sbd_filter = 1./float(sbd_avg_smp)*np.ones(int(sbd_avg_smp))
            self.m_sbd_lo_row = int(sbd_lo_row)
            self.m_sbd_hi_row = int(sbd_hi_row)
            
        self.m_calib_p0 = float(calib_p0)
        self.m_calib_p1 = float(calib_p1)
        self.m_calib_p2 = float(calib_p2)

        if laser_eventcode==None:
            self.m_laser_eventcode = 0
        else:
            self.m_laser_eventcode = int(laser_eventcode)

        if ref_db==None:
            self.m_ref_db = None
        else:
            self.m_ref_db = ttref_db(ref_db)
            
        if filter_wts==None:
            #  2um Si3N4
            #
            #    Generated by grabbing the leading edge of a good-looking signal,
            #      adding an offset so they sum to zero, scaling to give a
            #      unity response to the signal, and reversing the order
            #      (as the convolve() operator requires).
            #
            self.m_filter_wts = [0.021271,0.021420,0.021300,0.020841,0.019002,0.018521,0.017709,0.016917,0.016136,0.015775,0.016159,0.016625,0.018421,0.019124,0.019940,0.020863,0.022695,0.023021,0.022870,0.023539,0.022878,0.022498,0.021527,0.020684,0.018432,0.017906,0.017220,0.016278,0.015805,0.015476,0.016189,0.016698,0.018463,0.019164,0.020866,0.022218,0.023980,0.025467,0.026288,0.027654,0.027553,0.028020,0.027497,0.027532,0.027345,0.026183,0.024828,0.023994,0.022470,0.021380,0.020708,0.017988,0.015344,0.013443,0.012679,0.010346,0.008949,0.008011,0.005592,0.004422,0.003823,0.002964,0.001387,0.000745,0.000476,-0.000693,-0.001316,-0.001854,-0.003055,-0.004322,-0.005619,-0.007256,-0.008500,-0.009928,-0.010720,-0.012438,-0.014539,-0.015999,-0.016426,-0.017158,-0.017685,-0.020060,-0.022255,-0.023479,-0.024659,-0.025915,-0.027599,-0.030440,-0.033650,-0.035092,-0.036712,-0.038233,-0.039815,-0.041164,-0.042795,-0.042928,-0.043287,-0.043684,-0.042911,-0.042691,-0.041668,-0.041339,-0.040931,-0.039619,-0.038069,-0.036695,-0.036167,-0.035128,-0.034187,-0.032843]
        else:
            l = filter_wts.split()
            self.m_filter_wts = np.zeros(len(l))
            for i in range(len(l)):
                self.m_filter_wts[i] = float(l[i])
            
        self.m_put_spectrum_smp = int(put_spectrum_smp)
            
        #  Cumulative averaged dark region
        self.m_drk_initial = True
        self.m_drk = 32.*(float(sig_hi_row)-float(sig_lo_row))*np.ones(self.m_hi_column-self.m_lo_column)

        #  Spectrum w/o x-rays
        self.m_sig_ref_initial = True
        self.m_sig_ref = np.zeros(self.m_hi_column-self.m_lo_column)
        
        #  Sideband region also w/o x-rays
        self.m_sbd_ref_initial = True
        self.m_sbd_ref = np.zeros(self.m_hi_column-self.m_lo_column)

        self.m_debug = debug
        
        print 'Using %s'%self.m_det_name
        
    #-------------------
    #  Public methods --
    #-------------------

    def beginjob( self, evt, env ) :
        """This method is called once at the beginning of the job. It should
        do a one-time initialization possible extracting values from event
        data (which is a Configure object) or environment.

        @param evt    event data object
        @param env    environment object
        """

    def beginrun( self, evt, env ) :
        """This optional method is called if present at the beginning 
        of the new run.

        @param evt    event data object
        @param env    environment object
        """

    def begincalibcycle( self, evt, env ) :
        """This optional method is called if present at the beginning 
        of the new calibration cycle.

        @param evt    event data object
        @param env    environment object
        """

    def event( self, evt, env ) :
        """This method is called for every L1Accept transition.

        @param evt    event data object
        @param env    environment object
        """

        bykick = False
        laser  = not self.m_laser_eventcode>0

        try:
            evrdata = evt.getEvrData("NoDetector-0|Evr-0")
            if evrdata==None:
                print "EVR data missing"
                return
        except:
            print "Error fetching EVR data"
            return

        for i in range (evrdata.numFifoEvents()):
            if evrdata.fifoEvent(i).EventCode==_BYKICK_CODE:
                bykick = True
            if evrdata.fifoEvent(i).EventCode==self.m_laser_eventcode:
                laser = True
            if evrdata.fifoEvent(i).EventCode==-self.m_laser_eventcode:
                laser = False

        if laser==False:
            return  # No continuum
        
        try:
            frame = evt.get(xtc.TypeId.Type.Id_Frame, self.m_det_name)
            if frame==None:
                print "Frame data missing"
                return
        except:
            print "Error fetching timetool camera data"
            return

        image = frame.data()
        drk_proj = np.sum(image[self.m_drk_lo_row:self.m_drk_hi_row,
                                self.m_lo_column:self.m_hi_column],axis=0)
        if self.m_drk_initial:
            self.m_drk_initial=False
            self.m_drk = drk_proj
        else:
            self.m_drk = (1-self.m_drk_convergence)*self.m_drk + self.m_drk_convergence*drk_proj

#
#  Opal cameras have a small coherent noise contribution
#    Even (odd) pixels are correlated on the same quadrant
#
#        drk_proj = drk_proj - self.m_drk
#        drk_even_odd = np.reshape(drk_proj,(2,-1))
#        drk_even_odd_proj = np.mean(drk_even_odd,axis=1)
#        print 'drk even/odd ',
#        print drk_even_odd_proj
                
        sig_proj = np.sum(image[self.m_sig_lo_row:self.m_sig_hi_row,
                                self.m_lo_column:self.m_hi_column],axis=0)
        sig_proj = sig_proj - self.m_drk

        if self.m_use_sbd:
            sbd_proj = np.sum(image[self.m_sbd_lo_row:self.m_sbd_hi_row,
                                    self.m_lo_column:self.m_hi_column],axis=0)
            sbd_proj = sbd_proj - self.m_drk

        if self.m_ref_db!=None:
            self.m_sig_ref = self.m_ref_db.lookup(evt.getTime())[self.m_lo_column:self.m_hi_column]
            if self.m_sig_ref==None:
                return
        elif bykick:
            if self.m_sig_ref_initial:
                self.m_sig_ref_initial=False
                self.m_sig_ref = sig_proj
            else:
                self.m_sig_ref = (1-self.m_ref_convergence)*self.m_sig_ref + self.m_ref_convergence*sig_proj
                if self.m_use_sbd:
                    self.m_sbd_ref = (1-self.m_ref_convergence)*self.m_sbd_ref + self.m_ref_convergence*sbd_proj
                    
            evt.put(self.m_sig_ref, 'timetool:refspec')
            return  # Reference spectrum accumulated
        elif self.m_sig_ref_initial:
            return  # No reference spectrum

        sign_proj = sig_proj/self.m_sig_ref - np.ones(len(sig_proj))

        if self.m_use_sbd:
            n = len(self.m_sbd_filter)
            m = len(sbd_proj)
            sbdn_proj = sbd_proj/self.m_sbd_ref - np.ones(m)
            sbd_avg   = np.convolve(self.m_sbd_filter, sbdn_proj, 'full')[n/2:n/2+m]
            sign_proj = np.subtract(sign_proj,sbd_avg)
            
        sig_filtered = np.convolve(self.m_filter_wts, sign_proj, 'valid')

        sig_pos  = np.argmax(sig_filtered)
        sig_ampl = sig_filtered[sig_pos] 

        if sig_ampl>0:
            #  
            #  Fit the top 20%
            #    I couldn't find a good routine for seeking this range
            #
            fit_range = [0,len(sig_filtered)]
            fit_limit = sig_ampl*0.8
            for i in range(sig_pos,-1,-1):
                if sig_filtered[i] < fit_limit:
                    fit_range[0] = i+1
                    break
            for i in range(sig_pos,len(sig_filtered),1):
                if sig_filtered[i] < fit_limit:
                    fit_range[1] = i
                    break

            p2_fit = np.polyfit(range(fit_range[0]-sig_pos,
                                      fit_range[1]-sig_pos),
                                sig_filtered[fit_range[0]:fit_range[1]],2)
            
            position  = float(sig_pos) - p2_fit[1]/p2_fit[0]
            amplitude = p2_fit[2] - p2_fit[1]*p2_fit[1]/p2_fit[0]
        else:
            position  = float(sig_pos)
            amplitude = sig_ampl
            
        raw_pos = sig_pos + len(self.m_filter_wts)-1

        ns  = self.m_put_spectrum_smp
        mgn = ns-50
        if ns>0:
            if raw_pos<mgn:
                dtrans = np.zeros(ns)
                dtrans[mgn-raw_pos:ns] = sign_proj[0:raw_pos+ns-mgn]
            elif raw_pos+ns-mgn>len(sign_proj):
                dtrans = np.zeros(ns)
                dtrans[0:len(sign_proj)-raw_pos+mgn] = sign_proj[raw_pos-mgn:]
            else:
                dtrans = sign_proj[raw_pos-mgn:raw_pos+ns-mgn] 
            
            evt.put(dtrans   ,'timetool:dtrans')

        sig_pos = self.m_calib_p0 + self.m_calib_p1*float(sig_pos) + self.m_calib_p2*float(sig_pos*sig_pos)

        position = self.m_calib_p0 + self.m_calib_p1*position + self.m_calib_p2*position*position

        if self.m_debug:
            print 'ampl %f  pos %f'%(sig_ampl,sig_pos)
            print sig_proj[:100]
            print 'ref'
            print self.m_sig_ref[:100]
            
        evt.put(sig_pos  ,'timetool:position_unfit')
        evt.put(sig_ampl ,'timetool:amplitude_unfit')
        evt.put(position ,'timetool:position')
        evt.put(amplitude,'timetool:amplitude')

    def endcalibcycle( self, evt, env ) :
        """This optional method is called if present at the end of the 
        calibration cycle.
        
        @param evt    event data object
        @param env    environment object
        """

    def endrun( self, evt, env ) :
        """This optional method is called if present at the end of the run.
        
        @param evt    event data object
        @param env    environment object
        """

    def endjob( self, evt, env ) :
        """This method is called at the end of the job. It should do 
        final cleanup, e.g. close all open files.
        
        @param evt    event data object
        @param env    environment object
        """

