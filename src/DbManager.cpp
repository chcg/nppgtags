/**
 *  \file
 *  \brief  GTags database manager
 *
 *  \author  Pavel Nedev <pg.nedev@gmail.com>
 *
 *  \section COPYRIGHT
 *  Copyright(C) 2014-2022 Pavel Nedev
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


#include <windows.h>
#include "DbManager.h"
#include "INpp.h"
#include "GTags.h"
#include "Cmd.h"
#include "CmdEngine.h"
#include "ResultWin.h"


namespace GTags
{


/**
 *  \brief
 */
GTagsDb::GTagsDb(const CPath& dbPath, bool writeEn) : _path(dbPath), _writeLock(writeEn)
{
    if (!_cfg.LoadFromFolder(dbPath))
        _cfg = GTagsSettings._genericDbCfg;

    _readLocks = writeEn ? 0 : 1;
}


/**
 *  \brief
 */
void GTagsDb::Update(const CPath& file)
{
    CmdPtr_t cmd(new Cmd(UPDATE_SINGLE, this->shared_from_this(), NULL, file.C_str()));
    CmdEngine::Run(cmd, dbUpdateCB);
}


/**
 *  \brief
 */
void GTagsDb::ScheduleUpdate(const CPath& file)
{
    std::list<CPath>::reverse_iterator iFile;
    for (iFile = _updateList.rbegin(); iFile != _updateList.rend(); ++iFile)
        if (*iFile == file)
            return;

    _updateList.push_back(file);
}


/**
 *  \brief
 */
bool GTagsDb::lock(bool writeEn)
{
    if (writeEn)
    {
        if (_writeLock || _readLocks)
            return false;

        _writeLock = true;
    }
    else
    {
        if (_writeLock)
            return false;

        ++_readLocks;
    }

    return true;
}


/**
 *  \brief
 */
bool GTagsDb::unlock()
{
    if (_writeLock)
    {
        _writeLock = false;
    }
    else if (_readLocks > 0)
    {
        --_readLocks;
        return !_readLocks;
    }

    return true;
}


/**
 *  \brief
 */
void GTagsDb::runScheduledUpdate()
{
    if (_updateList.empty())
        return;

    lock(true);

    CPath file = *(_updateList.begin());
    _updateList.erase(_updateList.begin());

    Update(file);
}


/**
 *  \brief
 */
void GTagsDb::dbUpdateCB(const CmdPtr_t& cmd)
{
    if (cmd->Status() == RUN_ERROR)
    {
        MessageBox(INpp::Get().GetHandle(), _T("Running GTags failed"), cmd->Name(), MB_OK | MB_ICONERROR);
    }
    else if (cmd->Result())
    {
        CText msg(cmd->Result());
        MessageBox(INpp::Get().GetHandle(), msg.C_str(), cmd->Name(), MB_OK | MB_ICONEXCLAMATION);
    }

    cmd->Db()->unlock();
    cmd->Db()->runScheduledUpdate();

    if (cmd->Status() == OK)
        ResultWin::NotifyDBUpdate(cmd);
}


/**
 *  \brief
 */
const DbHandle& DbManager::RegisterDb(const CPath& dbPath)
{
    bool success;

    return lockDb(dbPath, true, &success);
}


/**
 *  \brief
 */
bool DbManager::UnregisterDb(const DbHandle& db)
{
    if (!db)
        return false;

    bool ret = false;

    for (std::list<DbHandle>::iterator dbi = _dbList.begin(); dbi != _dbList.end(); ++dbi)
    {
        if (db == *dbi)
        {
            if (db->unlock())
            {
                ret = deleteDb(db->_path);
                _dbList.erase(dbi);
            }

            break;
        }
    }

    return ret;
}


/**
 *  \brief
 */
DbHandle DbManager::GetDb(const CPath& filePath, bool writeEn, bool* success)
{
    if (!success)
        return NULL;

    *success = false;

    CPath dbPath(filePath);
    size_t len = dbPath.StripFilename();

    for (; len; len = dbPath.DirUp())
        if (DbExistsInFolder(dbPath))
            break;

    if (len == 0)
        return NULL;

    return lockDb(dbPath, writeEn, success);
}


/**
 *  \brief
 */
DbHandle DbManager::GetDbAt(const CPath& dbPath, bool writeEn, bool* success)
{
    if (!success)
        return NULL;

    *success = false;

    if (!DbExistsInFolder(dbPath))
        return NULL;

    return lockDb(dbPath, writeEn, success);
}


/**
 *  \brief
 */
void DbManager::PutDb(const DbHandle& db)
{
    if (!db)
        return;

    for (std::list<DbHandle>::iterator dbi = _dbList.begin(); dbi != _dbList.end(); ++dbi)
    {
        if (db == *dbi)
        {
            if (db->unlock())
                db->runScheduledUpdate();

            break;
        }
    }
}


/**
 *  \brief
 */
bool DbManager::DbExistsInFolder(const CPath& folder)
{
    CPath db(folder);
    db += _T("GTAGS");
    return db.FileExists();
}


/**
 *  \brief
 */
bool DbManager::deleteDb(CPath& dbPath)
{
    BOOL ret = FALSE;

    dbPath += _T("GTAGS");
    if (dbPath.FileExists())
        ret = DeleteFile(dbPath.C_str());

    dbPath.StripFilename();
    dbPath += _T("GPATH");
    if (dbPath.FileExists())
        ret |= DeleteFile(dbPath.C_str());

    dbPath.StripFilename();
    dbPath += _T("GRTAGS");
    if (dbPath.FileExists())
        ret |= DeleteFile(dbPath.C_str());

    dbPath.StripFilename();
    dbPath += cPluginCfgFileName;
    if (dbPath.FileExists())
        ret |= DeleteFile(dbPath.C_str());

    return ret ? true : false;
}


/**
 *  \brief
 */
const DbHandle& DbManager::lockDb(const CPath& dbPath, bool writeEn, bool* success)
{
    for (std::list<DbHandle>::iterator dbi = _dbList.begin(); dbi != _dbList.end(); ++dbi)
    {
        if ((*dbi)->_path == dbPath)
        {
            *success = (*dbi)->lock(writeEn);
            return *dbi;
        }
    }

    DbHandle newDb(new GTagsDb(dbPath, writeEn));
    _dbList.push_back(newDb);

    *success = true;

    return *(_dbList.rbegin());
}

} // namespace GTags
