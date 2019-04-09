/**
 *  \file
 *  \brief  GTags command class
 *
 *  \author  Pavel Nedev <pg.nedev@gmail.com>
 *
 *  \section COPYRIGHT
 *  Copyright(C) 2015-2019 Pavel Nedev
 *
 *  \section LICENSE
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "Cmd.h"


namespace GTags
{

/**
 *  \brief
 */
Cmd::Cmd(CmdId_t id, const TCHAR* name, DbHandle db, ParserPtr_t parser,
        const TCHAR* tag, bool ignoreCase, bool regExp) :
        _id(id), _db(db), _parser(parser),
        _ignoreCase(ignoreCase), _regExp(regExp), _skipLibs(false), _status(CANCELLED)
{
    if (name)
        _name = name;

    if (tag)
        _tag = tag;
}


void Cmd::AppendToResult(const std::vector<char>& data)
{
    // remove \0 string termination
    if (!_result.empty())
        _result.pop_back();
    _result.insert(_result.cend(), data.begin(), data.end());
}

} // namespace GTags
