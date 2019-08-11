/***************************************************************************
 tag: Bert Willaert  Fri Sept 21 09:31:20 CET 2012  soem_robotiq_3Finger.cpp

 soem_robotiq_3Finger.cpp -  description
 -------------------
 begin                : Fri September 21 2012
 copyright            : (C) 2012 Bert Willaert
 email                : first.last@mech.kuleuven.be

 ***************************************************************************
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Lesser General Public            *
 *   License as published by the Free Software Foundation; either          *
 *   version 2.1 of the License, or (at your option) any later version.    *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 59 Temple Place,                                    *
 *   Suite 330, Boston, MA  02111-1307  USA                                *
 *                                                                         *
 ***************************************************************************/

#include "soem_maxpos.h"
#include <soem_master/soem_driver_factory.h>



using namespace RTT;

/*
Command     LowByte of Controlword[binary] State Transition
Shutdown    0xxx x110     2, 6, 8
Switch On   0xxx x111     3
Switch On & Enable Operation    0xxx 1111     3, 4 *1)
Disable Voltage     0xxx xx0x     7, 9, 10, 12
Quick Stop    0xxx x01x    7, 10, 11
Disable Operation     0xxx 0111     5
Enable Operation    0xxx 1111    4, 16
Fault Reset     0xxx xxxx -> 1xxx xxxx    15
*/

void SoemMaxPos::cw_reset(){//0xxx x110
  new_control_word.set(7,false);
  new_control_word.set(0,false);
  new_control_word.set(1);
  new_control_word.set(2);
}
void SoemMaxPos::cw_switch_on(){//0xxx x111
  new_control_word.set(7,false);
  new_control_word.set(0);
  new_control_word.set(1);
  new_control_word.set(2);
}
void SoemMaxPos::cw_switch_on_and_enable_operation(){//0xxx 1111
  new_control_word.set(7,false);
  new_control_word.set(0);
  new_control_word.set(1);
  new_control_word.set(2);
  new_control_word.set(3);
}
void SoemMaxPos::cw_disable_voltage(){// 0xxx xx0x
  new_control_word.set(7,false);
  new_control_word.set(1,false);
}
void SoemMaxPos::cw_quick_stop(){// 0xxx x01x
  new_control_word.set(7,false);
  new_control_word.set(2,false);
  new_control_word.set(1);
}
void SoemMaxPos::cw_disable_operation(){// 0xxx 0111
  new_control_word.set(7,false);
  new_control_word.set(3,false);
  new_control_word.set(0);
  new_control_word.set(1);
  new_control_word.set(2);
}
void SoemMaxPos::cw_enable_operation(){// 0xxx 1111
  new_control_word.set(7,false);
  new_control_word.set(0);
  new_control_word.set(1);
  new_control_word.set(2);
  new_control_word.set(3);
}
void SoemMaxPos::cw_fault_reset(){// 1xxx xxxx (should be 0xxx xxxx before)
  new_control_word.set(7);
}

SoemMaxPos::SoemMaxPos(ec_slavet* mem_loc) :
  soem_master::SoemDriver(mem_loc)
, downsample(10)
, rev_position_ratio(1.0)
, rev_velocity_ratio(1.0)

{
  m_service->doc(std::string("MaxPos") + std::string(
                   m_datap->name) );

  m_service->addPort("status",status_word_outport).doc("written if values changes");
  m_service->addPort("digital_inputs",digital_inputs_outport).doc("written if values changes");
  m_service->addPort("touch_probe",touch_probe_status_outport).doc("written if values changes");

  m_service->addPort("position",position_outport);
  m_service->addPort("position_ros",position_outport_ds).doc("downsapled port");
  m_service->addPort("velocity",velocity_outport);
  m_service->addPort("velocity_ros",velocity_outport_ds).doc("downsapled port");
  m_service->addPort("torque",torque_outport);
  m_service->addPort("torque_ros",torque_outport_ds).doc("downsapled port");

  m_service->addProperty("rev_position_ratio",rev_position_ratio).doc("ratio revolution/(angle or meters)");
  m_service->addProperty("rev_velocity_ratio",rev_velocity_ratio).doc("ratio to convert rpm to angle/s or meters/s");


  m_service->addProperty("Downsample",downsample).doc("Downsaple ratio for ds (pos/vel/torque) ports, if 0 will disable it");

  //operations for escaling the state machine
  m_service->addOperation("reset", &SoemMaxPos::cw_reset, this, RTT::OwnThread).doc(
        "First command to be issued");
  m_service->addOperation("switch_on", &SoemMaxPos::cw_switch_on, this, RTT::OwnThread).doc(
        "Second command to be issued");
  m_service->addOperation("enable_operation", &SoemMaxPos::cw_enable_operation, this, RTT::OwnThread).doc(
        "Third command to be issued");
  m_service->addOperation("switch_on_and_enable_operation", &SoemMaxPos::cw_switch_on_and_enable_operation, this, RTT::OwnThread).doc(
        "two commands together");
  m_service->addOperation("disable_voltage", &SoemMaxPos::cw_disable_voltage, this, RTT::OwnThread).doc(
        "check manual");
  m_service->addOperation("disable_operation", &SoemMaxPos::cw_disable_operation, this, RTT::OwnThread).doc(
        "check manual");
  m_service->addOperation("quick_stop", &SoemMaxPos::cw_quick_stop, this, RTT::OwnThread).doc(
        "check manual");
  m_service->addOperation("fault_reset", &SoemMaxPos::cw_fault_reset, this, RTT::OwnThread).doc(
        "check manual");


  m_service->addOperation("set_mode_of_operation", &SoemMaxPos::set_mode_of_operation, this, RTT::OwnThread).doc(
        "change operational mode, returns true on success").arg("desired mode of operation"," values\n"
                                                                "/t/t1 Profile Position Mode (PPM) -not implemented \n"
                                                                "/t/t3 Profile Velocity Mode (PVM) -not implemented \n"
                                                                "/t/t6 Homing Mode (HMM) -not implemented n"
                                                                "/t/t8 Cyclic Synchronous Position Mode (CSP)\n"
                                                                "/t/t9 Cyclic Synchronous Velocity Mode (CSV)\n"
                                                                "/t/t10 Cyclic Synchronous Torque Mode (CST)");
}

bool SoemMaxPos::set_mode_of_operation(int mode)
{
  if (mode!=8 && mode!=9 && mode!=10){
      Logger::In in(this->getName());
    log(Error)<< m_datap->name<<" : requested mode is not implemented, only 8,9,10 currently implemented." << endlog();
    }
  int8 value=mode;
  int8 new_value=0;
  int n_of_bytes=sizeof(new_value);
  int retval;

  retval = ec_SDOwrite(m_slave_nr, 0x6060, 0x00, FALSE, sizeof(value), &value, EC_TIMEOUTSAFE);
  retval = ec_SDOread(m_slave_nr,  0x6061, 0x00, FALSE, &n_of_bytes , &new_value, EC_TIMEOUTSAFE);

  if (value!=new_value)
    Logger::In in(this->getName());
  log(Error)<< m_datap->name<<" : not able to set requested mode of operation." << endlog();
  return false;
  return true;

}

bool SoemMaxPos::configure()
{	 
  // Get period of the owner of the service, i.e. the soem master
  //this is not really needed in there are no trajectory inside the component
  //RTT::TaskContext *upla = m_service->getOwner();
  //std::cout << m_name << ": Name of owner: " << upla->getName() << std::endl;
  //ts_ = upla->getPeriod();
  //std::cout << m_name << ": Period of owner: " << ts_ << std::endl;
  // Set initial configuration parameters

  return true;
  iteration=0;
}



void SoemMaxPos::update()
{
std_msgs::UInt32 digital_inputs;
std_msgs::UInt16 touch_probe_status;
  // ****************************
  // *** Read data from slave ***
  // ****************************

  double position= (double)((read_mgs*) (m_datap->inputs))->position_actual_value;//rev
  double velocity= (double)((read_mgs*) (m_datap->inputs))->velocity_actual_value;//rpm
  double torque= (double)((read_mgs*) (m_datap->inputs))->torque_actual_value;//?
  position/=rev_position_ratio;
  velocity/=rev_velocity_ratio;

  uint16 status_word_uint= ((read_mgs*) (m_datap->inputs))->status_word;
  std::bitset<16> status_word(status_word_uint);
  touch_probe_status.data= ((read_mgs*) (m_datap->inputs))->touch_probe_status;
  digital_inputs.data= ((read_mgs*) (m_datap->inputs))->digital_inputs;

  //writes on port only if new values are recieved
  if  (status_word!=last_status_word){//request to move in the device state machine
      std::cout<<"status_word\t"<<status_word.to_string()<<"\n"<<std::endl;
      status_word_msg.data=status_word.to_string();
      status_word_outport.write(status_word_msg);
      last_status_word=status_word;

    }
  if  (digital_inputs.data!=last_digital_inputs.data){
      digital_inputs_outport.write(digital_inputs);
      last_digital_inputs=digital_inputs;
    }
  if  (touch_probe_status.data!=last_touch_probe_status.data){
      touch_probe_status_outport.write(touch_probe_status);
      last_touch_probe_status=touch_probe_status;
    }

  position_outport.write(position);
  velocity_outport.write(velocity);
  torque_outport.write(torque);
  if (downsample!=0 && iteration==downsample){
       std_msgs::Float32 d;
       d.data=position;position_outport_ds.write(d);
       d.data=velocity;velocity_outport_ds.write(d);
       d.data=torque;torque_outport_ds.write(d);
    }


  // ***************************
  // *** Write data to slave ***
  // ***************************


  ((control_msg*) (m_datap->outputs))->control_word =(unsigned short)new_control_word.to_ulong();


}


namespace
{
  soem_master::SoemDriver* createSoemMaxPos(ec_slavet* mem_loc)
  {
    return new SoemMaxPos(mem_loc);
  }
  const bool registered0 =
      soem_master::SoemDriverFactory::Instance().registerDriver("MAXPOS",
                                                                createSoemMaxPos);

}

