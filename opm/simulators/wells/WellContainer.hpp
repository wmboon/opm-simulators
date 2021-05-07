/*
  Copyright 2021 Equinor ASA

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OPM_WELL_CONTAINER_HEADER_INCLUDED
#define OPM_WELL_CONTAINER_HEADER_INCLUDED

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace Opm {

template <class T>
class WellContainer {
public:

    std::size_t size() const {
        return this->data.size();
    }

    void add(const std::string& name, T&& value) {
        if (index_map.count(name) != 0)
            throw std::logic_error("An object with name: " + name + " already exists in container");

        this->index_map.emplace(name, this->data.size());
        this->data.push_back(std::forward<T>(value));
    }

    bool has(const std::string& name) const {
        return (index_map.count(name) != 0);
    }


    void update(const std::string& name, T&& value) {
        auto index = this->index_map.at(name);
        this->data[index] = std::forward<T>(value);
    }

    void update(std::size_t index, T&& value) {
        this->data.at(index) = std::forward<T>(value);
    }

    void copy_welldata(const WellContainer<T>& other) {
        if (this->index_map == other.index_map)
            this->data = other.data;
        else {
            for (const auto& [name, index] : this->index_map)
                this->update_if(index, name, other);
        }
    }

    void copy_welldata(const WellContainer<T>& other, const std::string& name) {
        auto this_index = this->index_map.at(name);
        auto other_index = other.index_map.at(name);
        this->data[this_index] = other.data[other_index];
    }

    T& operator[](std::size_t index) {
        return this->data.at(index);
    }

    const T& operator[](std::size_t index) const {
        return this->data.at(index);
    }

    T& operator[](const std::string& name) {
        auto index = this->index_map.at(name);
        return this->data[index];
    }

    const T& operator[](const std::string& name) const {
        auto index = this->index_map.at(name);
        return this->data[index];
    }

    void clear() {
        this->data.clear();
        this->index_map.clear();
    }

    typename std::vector<T>::const_iterator begin() const {
        return this->data.begin();
    }

    typename std::vector<T>::const_iterator end() const {
        return this->data.end();
    }

private:
    void update_if(std::size_t index, const std::string& name, const WellContainer<T>& other) {
        auto other_iter = other.index_map.find(name);
        if (other_iter == other.index_map.end())
            return;

        auto other_index = other_iter->second;
        this->data[index] = other.data[other_index];
    }


    std::vector<T> data;
    std::unordered_map<std::string, std::size_t> index_map;
};


}


#endif
