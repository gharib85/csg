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

#include "lammpsdatareader.h"
#include <boost/algorithm/string.hpp>
#include <vector>
#include <votca/csg/topology.h>
#include <votca/tools/elements.h>
#include <votca/tools/floatingpointcomparison.h>
#include <votca/tools/getline.h>

namespace votca {
namespace csg {
using namespace std;

/*****************************************************************************
 * Internal Helper Functions                                                 *
 *****************************************************************************/
vector<string> TrimCommentsFrom_(vector<string> fields) {
  vector<string> tempFields;
  for (const auto &field : fields) {
    if (field.at(0) == '#') {
      break;
    }
    tempFields.push_back(field);
  }
  return tempFields;
}

/*****************************************************************************
 * Public Facing Methods                                                     *
 *****************************************************************************/

// Data file should follow this format:
// https://lammps.sandia.gov/doc/2001/data_format.html
bool LAMMPSDataReader::ReadTopology(string file, Topology &top) {

  cout << endl;
  cout << "WARNING: The votca lammps data reader is only able to read ";
  cout << "lammps files formatted in the following styles:" << endl;
  cout << "angle" << endl;
  cout << "atom" << endl;
  cout << "bond" << endl;
  cout << "full" << endl;
  cout << "molecule" << endl;
  cout << endl;
  cout << "These styles use the following formats in the atom block:" << endl;
  cout << "atom-ID molecule-ID atom-type charge x y z" << endl;
  cout << "atom-ID molecule-ID atom-type charge x y z nx ny nz" << endl;
  cout << "atom-ID molecule-ID atom-type x y z" << endl;
  cout << "atom-ID molecule-ID atom-type x y z nx ny nz" << endl;
  cout << "atom-ID atom-type x y z" << endl;
  cout << "atom-ID atom-type x y z nx ny nz" << endl;
  cout << endl;

  topology_ = true;
  top.Cleanup();
  fl_.open(file.c_str());
  if (!fl_.is_open()) {
    throw std::ios_base::failure("Error on open topology file: " + file);
  }

  fname_ = file;

  NextFrame(top);

  fl_.close();
  for (Molecule *mol : top.Molecules()) {
    RenameMolecule(*mol);
  }

  return true;
}

bool LAMMPSDataReader::Open(const string &file) {
  fl_.open(file.c_str());
  if (!fl_.is_open()) {
    throw std::ios_base::failure("Error on open trajectory file: " + file);
  }
  fname_ = file;
  return true;
}

void LAMMPSDataReader::Close() { fl_.close(); }

bool LAMMPSDataReader::FirstFrame(Topology &top) {
  topology_ = false;
  NextFrame(top);
  return true;
}

bool LAMMPSDataReader::NextFrame(Topology &top) {

  string header;
  getline(fl_, header);
  string line;
  getline(fl_, line);
  while (!fl_.eof()) {

    tools::Tokenizer tok(line, " ");
    vector<string> fields = tok.ToVector();
    fields = TrimCommentsFrom_(fields);
    // If not check the size of the vector and parse according
    // to the number of fields
    if (fields.size() == 1) {
      MatchOneFieldLabel_(fields, top);
    } else if (fields.size() == 2) {
      MatchTwoFieldLabels_(fields, top);
    } else if (fields.size() == 3) {
      MatchThreeFieldLabels_(fields);
    } else if (fields.size() == 4) {
      MatchFourFieldLabels_(fields, top);
    } else if (fields.size() != 0) {
      string err = "Unrecognized line in lammps .data file:\n" + line;
      throw runtime_error(err);
    }
    getline(fl_, line);
  }
  return !fl_.eof();
}

/*****************************************************************************
 * Private Facing Methods                                                    *
 *****************************************************************************/

void LAMMPSDataReader::RenameMolecule(Molecule &mol) const {
  if (mol.getName() == "UNKNOWN") {
    std::map<std::string, Index> molname_map;
    for (const Bead *atom : mol.Beads()) {
      std::string atomname = atom->getName();
      std::string element = atomname.substr(0, 1);
      if (std::islower(atomname[1])) {
        element += atomname[1];
      }
      molname_map[element]++;
    }

    std::string molname = "";
    for (auto const &pair : molname_map) {
      molname += (pair.first + std::to_string(pair.second));
    }
    mol.setName(molname);
  }
}

bool LAMMPSDataReader::MatchOneFieldLabel_(vector<string> fields,
                                           Topology &top) {

  if (fields.at(0) == "Masses") {
    SortIntoDataGroup_("Masses");
    InitializeAtomAndBeadTypes_();
  } else if (fields.at(0) == "Atoms") {
    ReadAtoms_(top);
  } else if (fields.at(0) == "Bonds") {
    ReadBonds_(top);
  } else if (fields.at(0) == "Angles") {
    ReadAngles_(top);
  } else if (fields.at(0) == "Dihedrals") {
    ReadDihedrals_(top);
  } else if (fields.at(0) == "Impropers") {
    cout << endl;
    cout << "WARNING Impropers are not currently supported, skipping." << endl;
    cout << endl;
    // Impropers are not yet supported
    SkipImpropers_();
  } else {
    return false;
  }
  return true;
}

bool LAMMPSDataReader::MatchTwoFieldLabels_(vector<string> fields,
                                            Topology &top) {

  string label = fields.at(0) + " " + fields.at(1);

  if (fields.at(1) == "atoms") {
    ReadNumOfAtoms_(fields, top);
  } else if (fields.at(1) == "bonds") {
    ReadNumOfBonds_(fields);
  } else if (fields.at(1) == "angles") {
    ReadNumOfAngles_(fields);
  } else if (fields.at(1) == "dihedrals") {
    ReadNumOfDihedrals_(fields);
  } else if (fields.at(1) == "impropers") {
    ReadNumOfImpropers_(fields);
  } else if (label == "Pair Coeffs") {
    SortIntoDataGroup_("Pair Coeffs");
  } else if (label == "Bond Coeffs") {
    SortIntoDataGroup_("Bond Coeffs");
  } else if (label == "Angle Coeffs") {
    SortIntoDataGroup_("Angle Coeffs");
  } else if (label == "Improper Coeffs") {
    SortIntoDataGroup_("Improper Coeffs");
  } else {
    return false;
  }
  return true;
}

bool LAMMPSDataReader::MatchThreeFieldLabels_(vector<string> fields) {
  string label = fields.at(1) + " " + fields.at(2);
  if (label == "atom types") {
    ReadNumTypes_(fields, "atom");
  } else if (label == "bond types") {
    ReadNumTypes_(fields, "bond");
  } else if (label == "angle types") {
    ReadNumTypes_(fields, "angle");
  } else if (label == "dihedral types") {
    ReadNumTypes_(fields, "Dihedral");
  } else if (label == "improper types") {
    ReadNumTypes_(fields, "Improper");
  } else {
    return false;
  }
  return true;
}

bool LAMMPSDataReader::MatchFourFieldLabels_(vector<string> fields,
                                             Topology &top) {
  string label = fields.at(2) + " " + fields.at(3);
  if (label == "xlo xhi") {
    ReadBox_(fields, top);
  } else {
    return false;
  }
  return true;
}

void LAMMPSDataReader::InitializeAtomAndBeadTypes_() {
  if (!data_.count("Masses")) {
    string err = "Masses must first be parsed before the atoms can be read.";
    throw runtime_error(err);
  }

  Index index = 0;
  tools::Elements elements;
  for (auto mass : data_["Masses"]) {
    // Determine the mass associated with the atom
    double mass_atom_bead = stod(mass.at(1));
    string baseName;
    if (elements.isMassAssociatedWithElement(mass_atom_bead, 0.01)) {
      baseName = elements.getEleShortClosestInMass(mass_atom_bead, 0.01);
    } else {
      baseName = "Bead";
      cout << "Unable to associate mass " << mass.at(1)
           << " with element assuming pseudo atom, assigning name "
           << "Bead" << mass.at(0) << " ." << endl;
    }
    atomtypes_[index] = baseName + mass.at(0);
    ++index;
  }
}

void LAMMPSDataReader::ReadBox_(vector<string> fields, Topology &top) {
  Eigen::Matrix3d m = Eigen::Matrix3d::Zero();
  m(0, 0) = stod(fields.at(1)) - stod(fields.at(0));

  for (Index i = 1; i < 3; ++i) {
    string line;
    getline(fl_, line);
    tools::Tokenizer tok(line, " ");
    tok.ConvertToVector(fields);
    if (fields.size() != 4) {
      throw runtime_error("invalid box format in the lammps data file");
    }

    m(i, i) = stod(fields.at(1)) - stod(fields.at(0));
  }
  top.setBox(m * tools::conv::ang2nm);
}

void LAMMPSDataReader::SortIntoDataGroup_(string tag) {
  string line;
  getline(fl_, line);
  getline(fl_, line);

  vector<vector<string>> group;
  string data_elem;
  while (!line.empty()) {
    tools::Tokenizer tok(line, " ");
    vector<string> fields = tok.ToVector();
    group.push_back(fields);
    getline(fl_, line);
  }

  data_[tag] = group;
}

void LAMMPSDataReader::ReadNumTypes_(vector<string> fields, string type) {
  numberOfDifferentTypes_[type] = stoi(fields.at(0));
}

void LAMMPSDataReader::ReadNumOfAtoms_(vector<string> fields, Topology &top) {
  numberOf_["atoms"] = stoi(fields.at(0));
  if (!topology_ && numberOf_["atoms"] != top.BeadCount()) {
    std::runtime_error("Number of beads in topology and trajectory differ");
  }
}

void LAMMPSDataReader::ReadNumOfBonds_(vector<string> fields) {
  numberOf_["bonds"] = stoi(fields.at(0));
}

void LAMMPSDataReader::ReadNumOfAngles_(vector<string> fields) {
  numberOf_["angles"] = stoi(fields.at(0));
}

void LAMMPSDataReader::ReadNumOfDihedrals_(vector<string> fields) {
  numberOf_["dihedrals"] = stoi(fields.at(0));
}

void LAMMPSDataReader::ReadNumOfImpropers_(vector<string> fields) {
  numberOf_["impropers"] = stoi(fields.at(0));
}

LAMMPSDataReader::lammps_format LAMMPSDataReader::determineDataFileFormat_(
    string line) {

  tools::Tokenizer tok(line, " ");
  vector<string> fields = tok.ToVector();
  lammps_format format;
  if (fields.size() == 5 || fields.size() == 8) {
    format = style_atomic;
  } else if (fields.size() == 6 || fields.size() == 9) {
    format = style_angle_bond_molecule;
  } else if (fields.size() == 7 || fields.size() == 10) {
    format = style_full;
  } else {
    throw runtime_error(
        "You have submitted a lammps data file with an "
        "unsupported format.");
  }
  return format;
}

void LAMMPSDataReader::ReadAtoms_(Topology &top) {

  if (data_.count("Masses") == 0) {
    string err =
        "You are attempting to read in the atom block before the masses, or "
        "you have failed to include the masses in the data file.";
    throw runtime_error(err);
  }

  string line;
  getline(fl_, line);
  getline(fl_, line);

  lammps_format format = determineDataFileFormat_(line);
  bool chargeRead = false;
  bool moleculeRead = false;
  if (format == style_angle_bond_molecule) {
    moleculeRead = true;
  }
  if (format == style_full) {
    moleculeRead = true;
    chargeRead = true;
  }

  map<Index, string> sorted_file;
  Index startingIndex;
  Index startingIndexMolecule = 0;
  istringstream issfirst(line);
  issfirst >> startingIndex;
  if (moleculeRead) {
    issfirst >> startingIndexMolecule;
  }
  sorted_file[startingIndex] = line;
  getline(fl_, line);
  boost::trim(line);

  Index atomId = 0;
  Index moleculeId = 0;
  while (!line.empty()) {
    istringstream iss(line);
    iss >> atomId;
    if (moleculeRead) {
      iss >> moleculeId;
    }
    sorted_file[atomId] = line;
    getline(fl_, line);
    boost::trim(line);
    if (atomId < startingIndex) {
      startingIndex = atomId;
    }
    if (moleculeId < startingIndexMolecule) {
      startingIndexMolecule = moleculeId;
    }
  }

  for (Index atomIndex = startingIndex;
       static_cast<size_t>(atomIndex - startingIndex) < sorted_file.size();
       ++atomIndex) {

    Index atomTypeId;
    double charge = 0;
    double x, y, z;

    istringstream iss(sorted_file[atomIndex]);
    iss >> atomId;
    if (moleculeRead) {
      iss >> moleculeId;
    } else {
      moleculeId = atomId;
    }
    iss >> atomTypeId;
    if (chargeRead) {
      iss >> charge;
    }
    iss >> x;
    iss >> y;
    iss >> z;

    // Exclusion list assumes beads start with ids of 0
    --atomId;
    --atomTypeId;
    moleculeId -= startingIndexMolecule;

    // We want to start with an index of 0 not 1
    // atomId;
    Bead *b;
    if (topology_) {

      atomIdToIndex_[atomId] = atomIndex - startingIndex;
      atomIdToMoleculeId_[atomId] = moleculeId;
      Molecule *mol;
      if (!molecules_.count(moleculeId)) {
        mol = top.CreateMolecule("UNKNOWN");
        molecules_[moleculeId] = mol;
      } else {
        mol = molecules_[moleculeId];
      }
      Index symmetry = 1;  // spherical

      double mass = stod(data_["Masses"].at(atomTypeId).at(1));

      if (Index(data_.at("Masses").size()) <= atomTypeId) {
        string err =
            "The atom block contains an atom of type " + to_string(atomTypeId) +
            " however, the masses are only specified for atoms up to type " +
            to_string(data_.at("Masses").size() - 1);
        throw runtime_error(err);
      }

      Index residue_index = moleculeId;
      if (residue_index >= top.ResidueCount()) {
        while ((residue_index - 1) >= top.ResidueCount()) {
          top.CreateResidue("DUM");
        }
        top.CreateResidue("DUM");
      }

      if (atomtypes_.count(atomTypeId) == 0) {
        string err =
            "Unrecognized atomTypeId, the atomtypes map "
            "may be uninitialized";
        throw runtime_error(err);
      }

      string bead_type_name = atomtypes_[atomTypeId];
      if (!top.BeadTypeExist(bead_type_name)) {
        top.RegisterBeadType(bead_type_name);
      }

      b = top.CreateBead(symmetry, bead_type_name, bead_type_name,
                         residue_index, mass, charge);

      mol->AddBead(b, bead_type_name);
      b->setMolecule(mol);

    } else {
      b = top.getBead(atomIndex - startingIndex);
    }

    Eigen::Vector3d xyz_pos(x, y, z);
    b->setPos(xyz_pos);
  }

  if (top.BeadCount() != numberOf_["atoms"]) {
    string err =
        "The number of atoms read in is not equivalent to the "
        "number of atoms indicated to exist in the lammps data file. \n"
        "Number of atoms that should exist " +
        to_string(numberOf_["atoms"]) + "\nNumber of atoms that were read in " +
        to_string(top.BeadCount()) + "\n";
    throw runtime_error(err);
  }
}

void LAMMPSDataReader::ReadBonds_(Topology &top) {
  string line;
  getline(fl_, line);
  getline(fl_, line);
  boost::trim(line);

  Index bondId;
  Index bondTypeId;
  Index atom1Id, atom2Id;

  Index bond_count = 0;
  while (!line.empty()) {

    if (topology_) {
      istringstream iss(line);
      iss >> bondId;
      iss >> bondTypeId;
      iss >> atom1Id;
      iss >> atom2Id;

      --atom1Id;
      --atom2Id;
      --bondTypeId;
      --bondId;

      Index atom1Index = atomIdToIndex_[atom1Id];
      Index atom2Index = atomIdToIndex_[atom2Id];

      Interaction *ic = new IBond(atom1Index, atom2Index);
      ic->setGroup("BONDS");
      ic->setIndex(bondId);
      auto b = top.getBead(atom1Index);
      auto mi = b->getMolecule();
      ic->setMolecule(atomIdToMoleculeId_[atom1Index]);
      top.AddBondedInteraction(ic);
      mi->AddInteraction(ic);
    }

    ++bond_count;
    getline(fl_, line);
    boost::trim(line);
  }

  if (bond_count != numberOf_["bonds"]) {
    string err =
        "The number of bonds read in is not equivalent to the "
        "number of bonds indicated to exist in the lammps data file. \n"
        "Number of bonds that should exist " +
        to_string(numberOf_["bonds"]) + "\nNumber of bonds that were read in " +
        to_string(bond_count) + "\n";
    throw runtime_error(err);
  }
}

void LAMMPSDataReader::ReadAngles_(Topology &top) {
  string line;
  getline(fl_, line);
  getline(fl_, line);
  boost::trim(line);

  Index angleId;
  Index angleTypeId;
  Index atom1Id, atom2Id, atom3Id;

  Index angle_count = 0;

  while (!line.empty()) {

    if (topology_) {
      istringstream iss(line);
      iss >> angleId;
      iss >> angleTypeId;
      iss >> atom1Id;
      iss >> atom2Id;
      iss >> atom3Id;

      --angleId;
      --atom1Id;
      --atom2Id;
      --atom3Id;
      --angleTypeId;

      Index atom1Index = atomIdToIndex_[atom1Id];
      Index atom2Index = atomIdToIndex_[atom2Id];
      Index atom3Index = atomIdToIndex_[atom3Id];

      Interaction *ic = new IAngle(atom1Index, atom2Index, atom3Index);
      ic->setGroup("ANGLES");
      ic->setIndex(angleId);
      auto b = top.getBead(atom1Index);
      auto mi = b->getMolecule();
      ic->setMolecule(atomIdToMoleculeId_[atom1Index]);
      top.AddBondedInteraction(ic);
      mi->AddInteraction(ic);
    }

    ++angle_count;

    getline(fl_, line);
    boost::trim(line);
  }

  if (angle_count != numberOf_["angles"]) {
    string err =
        "The number of angles read in is not equivalent to the "
        "number of angles indicated to exist in the lammps data file. \n"
        "Number of angles that should exist " +
        to_string(numberOf_["angles"]) +
        "\nNumber of angles that were read in " + to_string(angle_count) + "\n";
    throw runtime_error(err);
  }
}

void LAMMPSDataReader::SkipImpropers_() {
  string line;
  getline(fl_, line);
  getline(fl_, line);
  while (!line.empty()) {
    getline(fl_, line);
  }
}

void LAMMPSDataReader::ReadDihedrals_(Topology &top) {
  string line;
  getline(fl_, line);
  getline(fl_, line);
  boost::trim(line);

  Index dihedralId;
  Index dihedralTypeId;
  Index atom1Id, atom2Id, atom3Id, atom4Id;

  Index dihedral_count = 0;
  while (!line.empty()) {

    if (topology_) {
      istringstream iss(line);
      iss >> dihedralId;
      iss >> dihedralTypeId;
      iss >> atom1Id;
      iss >> atom2Id;
      iss >> atom3Id;
      iss >> atom4Id;

      --dihedralId;
      --atom1Id;
      --atom2Id;
      --atom3Id;
      --atom4Id;
      --dihedralTypeId;

      Index atom1Index = atomIdToIndex_[atom1Id];
      Index atom2Index = atomIdToIndex_[atom2Id];
      Index atom3Index = atomIdToIndex_[atom3Id];
      Index atom4Index = atomIdToIndex_[atom4Id];

      Interaction *ic =
          new IDihedral(atom1Index, atom2Index, atom3Index, atom4Index);
      ic->setGroup("DIHEDRALS");
      ic->setIndex(dihedralId);
      auto b = top.getBead(atom1Index);
      auto mi = b->getMolecule();
      ic->setMolecule(atomIdToMoleculeId_[atom1Index]);
      top.AddBondedInteraction(ic);
      mi->AddInteraction(ic);
    }
    ++dihedral_count;
    getline(fl_, line);
    boost::trim(line);
  }

  if (dihedral_count != numberOf_["dihedrals"]) {
    string err =
        "The number of dihedrals read in is not equivalent to the "
        "number of dihedrals indicated to exist in the lammps data file. \n"
        "Number of dihedrals that should exist " +
        to_string(numberOf_["dihedrals"]) +
        "\nNumber of dihedrals that were read in " + to_string(dihedral_count) +
        "\n";
    throw runtime_error(err);
  }
}
}  // namespace csg
}  // namespace votca
