/*
 *  cDriver_TextViewer.cc
 *  Avida
 *
 *  Created by Charles on 7/1/07
 *  Copyright 1999-2007 Michigan State University. All rights reserved.
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "cDriver_TextViewer.h"

#include "cAnalyze.h"
#include "cChangeList.h"
#include "cClassificationManager.h"
#include "cGenotype.h"
#include "cHardwareBase.h"
#include "cOrganism.h"
#include "cPopulation.h"
#include "cPopulationCell.h"
#include "cStats.h"
#include "cString.h"
#include "cStringList.h"
#include "cWorld.h"

#include "cDriverManager.h"
#include "cTextViewerManager.h"

#include <cstdlib>

using namespace std;

cDriver_TextViewer::cDriver_TextViewer(cWorld* world)
  : m_world(world), m_info(m_world->GetPopulation(), 12), m_done(false)
{
  // Setup the initial view mode (loaded from avida.cfg)
//  SetViewMode(world->GetConfig().VIEW_MODE.Get());
    
  cDriverManager::Register(static_cast<cAvidaDriver*>(this));
  world->SetDriver(this);
}

cDriver_TextViewer::~cDriver_TextViewer()
{
  cDriverManager::Unregister(static_cast<cAvidaDriver*>(this));
  delete m_world;
    
  ExitTextViewer(0);
}


void cDriver_TextViewer::Run()
{
  cClassificationManager& classmgr = m_world->GetClassificationManager();
  cPopulation& population = m_world->GetPopulation();
  cStats& stats = m_world->GetStats();
  
  const int ave_time_slice = m_world->GetConfig().AVE_TIME_SLICE.Get();
  const double point_mut_prob = m_world->GetConfig().POINT_MUT_PROB.Get();
  
  cAvidaContext ctx(m_world->GetRandom());
  
  while (!m_done) {
    if (cChangeList* change_list = population.GetChangeList()) {
      change_list->Reset();
    }
    
    m_world->GetEvents(ctx);
    if (m_done == true) break;
    
    // Increment the Update.
    stats.IncCurrentUpdate();
    
    // Handle all data collection for previous update.
    if (stats.GetUpdate() > 0) {
      // Tell the stats object to do update calculations and printing.
      stats.ProcessUpdate();
      
      // Update all the genotypes for the end of this update.
      for (cGenotype * cur_genotype = classmgr.ResetThread(0);
           cur_genotype != NULL && cur_genotype->GetThreshold();
           cur_genotype = classmgr.NextGenotype(0)) {
        cur_genotype->UpdateReset();
      }
    }
    
    
    // Process the update.
    const int UD_size = ave_time_slice * population.GetNumOrganisms();
    const double step_size = 1.0 / (double) UD_size;
    

    // Are we stepping through an organism?
    if (m_info.GetStepOrganism() != -1) {  // Yes we are!
                                            // Keep the viewer informed about the organism we are stepping through...
      for (int i = 0; i < UD_size; i++) {
        const int next_id = population.ScheduleOrganism();
        if (next_id == m_info.GetStepOrganism()) {
          NotifyUpdate();
//          m_view.NewUpdate();
          
          // This is needed to have the top bar drawn properly; I'm not sure why...
          static bool first_update = true;
          if (first_update) {
//            Refresh();
            first_update = false;
          }
        }
        population.ProcessStep(ctx, step_size, next_id);
      }
    }
    else {
      for (int i = 0; i < UD_size; i++) population.ProcessStep(ctx, step_size);
    }
    
    
    // end of update stats...
    population.CalcUpdateStats();
    
    
    // Setup the viewer for the new update.
    if (m_info.GetStepOrganism() == -1) {
      NotifyUpdate();
//      NewUpdate();
      
      // This is needed to have the top bar drawn properly; I'm not sure why...
      static bool first_update = true;
      if (first_update) {
//        Refresh();
        first_update = false;
      }
    }
    
    
    // Do Point Mutations
    if (point_mut_prob > 0 ) {
      for (int i = 0; i < population.GetSize(); i++) {
        if (population.GetCell(i).IsOccupied()) {
          population.GetCell(i).GetOrganism()->GetHardware().PointMutate(ctx, point_mut_prob);
        }
      }
    }
    
    // Exit conditons...
    if (population.GetNumOrganisms() == 0) m_done = true;
  }
}


void cDriver_TextViewer::SignalBreakpoint()
{
//  m_view.DoBreakpoint();
}


void cDriver_TextViewer::Flush()
{
  cStringList & out_list = m_info.GetOutList();
  cStringList & err_list = m_info.GetErrList();
  
  // And notify the output...
  while (out_list.GetSize() > 0) {
    NotifyOutput(out_list.Pop());
  }

  while (err_list.GetSize() > 0) {
    cString cur_string(err_list.Pop());
    // cur_string.Insert("! ");
    NotifyWarning(cur_string);
  }
}


bool cDriver_TextViewer::ProcessKeypress(int keypress)
{
//  return m_view.ProcessKeypress(keypress);
  return false;
}


void cDriver_TextViewer::RaiseException(const cString& in_string)
{
  NotifyWarning(in_string);
}

void cDriver_TextViewer::RaiseFatalException(int exit_code, const cString& in_string)
{
  NotifyError(in_string);
  exit(exit_code);
}

void cDriver_TextViewer::NotifyUpdate()
{
  // @CAO What should happen on an update?
  // Update bar at top of screen
  // Update current view
  // Check for Inputs...
}

void cDriver_TextViewer::NotifyComment(const cString& in_string)
{
  // @CAO Do anything special if we know its just a normal comment?
  Notify(in_string);
}

void cDriver_TextViewer::NotifyWarning(const cString& in_string)
{
  cString out_string(in_string);
  out_string.Insert("Warning: ");
  Notify(out_string);
}

void cDriver_TextViewer::NotifyError(const cString& in_string)
{
  cString out_string(in_string);
  out_string.Insert("Error: ");
  Notify(out_string);
  ExitTextViewer(1);
}

void cDriver_TextViewer::NotifyOutput(const cString& in_string)
{
  // @CAO Do anything special if we know its from cout/cerr?
  Notify(in_string);
}

void cDriver_TextViewer::Notify(const cString& in_string)
{
  // @CAO We need to display this!
}



void ExitTextViewer(int exit_code)
{
  signal(SIGINT, SIG_IGN);           // Ignore all future interupts.
//  mvcur(0, COLS - 1, LINES - 1, 0);  // Move curser to the lower left.
//  endwin();                          // Restore terminal mode.

  printf ("Exit Code: %d\n", exit_code);

  exit(exit_code);
}
