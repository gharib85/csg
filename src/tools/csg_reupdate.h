/*
 * Copyright 2009-2019 The VOTCA Development Team (http://www.votca.org)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _VOTCA_CSG_REUPDATE_H
#define _VOTCA_CSG_REUPDATE_H
#include "../../include/votca/csg/csgapplication.h"
#include "../../include/votca/csg/potentialfunctions/potentialfunction.h"
#include "../../include/votca/csg/potentialfunctions/potentialfunctioncbspl.h"
#include "../../include/votca/csg/potentialfunctions/potentialfunctionlj126.h"
#include "../../include/votca/csg/potentialfunctions/potentialfunctionljg.h"
#include "../../include/votca/csg/topologyreader.h"
#include <boost/program_options.hpp>
#include <votca/tools/histogramnew.h>
#include <votca/tools/property.h>
#include <votca/tools/table.h>

using namespace votca::csg;
using namespace votca::tools;

struct PotentialInfo {

  PotentialInfo(votca::Index index, bool bonded_, votca::Index vec_pos_,
                std::string &param_in_ext_, Property *options,
                bool gentable = false);

  votca::Index potentialIndex;
  bool bonded;
  PotentialFunction *ucg;
  votca::Index vec_pos;
  std::pair<votca::Index, votca::Index> beadTypes;

  std::string potentialName;
  std::string potentialFunction;
  std::string type1, type2;

  double rmin, rcut;

  Property *_options;
};

class CsgREupdate : public CsgApplication {
 public:
  std::string ProgramName() override { return "csg_reupdate"; }
  void HelpText(std::ostream &out) override {
    out << "computes relative entropy update.";
  }

  bool DoTrajectory() override { return true; }

  bool DoMapping() override { return false; }

  bool DoThreaded() override { return true; }
  bool SynchronizeThreads() override { return false; }

  bool NeedsTopology() override { return false; }

  void Initialize() override;
  bool EvaluateOptions() override;
  void BeginEvaluate(Topology *top, Topology *top_atom = nullptr) override;
  void LoadOptions(const std::string &file);

  void Run() override;

  void EndEvaluate() override;
  CsgApplication::Worker *ForkWorker(void) override;
  void MergeWorker(Worker *worker) override;

 private:
 protected:
  Property _options;
  std::vector<Property *> _nonbonded;

  using PotentialContainer = std::vector<PotentialInfo *>;
  PotentialContainer _potentials;

  votca::Index _nlamda;
  Eigen::VectorXd _lamda;
  // _HS is a symmetric matrix
  Eigen::MatrixXd _HS;
  Eigen::VectorXd _DS;
  Eigen::VectorXd _dUFrame;
  bool _hessian_check;

  double _UavgAA;
  double _UavgCG;
  double _beta;
  double _relax;
  votca::Index _nframes;

  bool _gentable;
  bool _dosteep;

  std::vector<Table *> _aardfs;
  std::vector<double *> _aardfnorms;

  // file extension for the inputs/outputs
  std::string _param_in_ext, _param_out_ext;
  std::string _pot_out_ext;
  std::string _rdf_ext;

  void WriteOutFiles();
  void EvalBonded(Topology *conf, PotentialInfo *potinfo);
  void EvalNonbonded(Topology *conf, PotentialInfo *potinfo);

  // Compute Avg U, dU, and d2U values in reference AA ensemble
  void AAavgBonded(PotentialInfo *potinfo);
  void AAavgNonbonded(PotentialInfo *potinfo);

  // Formulates _HS dlamda = - _DS system of Lin Eq.
  void REFormulateLinEq();

  // Solve _HS dlamda = - _DS and update _lamda
  void REUpdateLamda();
};

class CsgREupdateWorker : public CsgApplication::Worker {
 public:
  ~CsgREupdateWorker() override = default;

  Property _options;
  std::vector<Property *> _nonbonded;

  using PotentialContainer = std::vector<PotentialInfo *>;
  PotentialContainer _potentials;

  votca::Index _nlamda;
  Eigen::VectorXd _lamda;
  Eigen::MatrixXd _HS;
  Eigen::VectorXd _DS;
  Eigen::VectorXd _dUFrame;

  double _UavgCG;
  double _beta;
  votca::Index _nframes;

  void EvalConfiguration(Topology *conf, Topology *conf_atom) override;
  void EvalBonded(Topology *conf, PotentialInfo *potinfo);
  void EvalNonbonded(Topology *conf, PotentialInfo *potinfo);
};

#endif /* _VOTCA_CSG_REUPDATE_H */
