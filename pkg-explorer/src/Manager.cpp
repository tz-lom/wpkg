//===============================================================================
// Copyright:	Copyright (c) 2013-2015 Made to Order Software Corp.
//
// All Rights Reserved.
//
// The source code in this file ("Source Code") is provided by Made to Order Software Corporation
// to you under the terms of the GNU General Public License, version 2.0
// ("GPL").  Terms of the GPL can be found in doc/GPL-license.txt in this distribution.
// 
// By copying, modifying or distributing this software, you acknowledge
// that you have read and understood your obligations described above,
// and agree to abide by those obligations.
// 
// ALL SOURCE CODE IN THIS DISTRIBUTION IS PROVIDED "AS IS." THE AUTHOR MAKES NO
// WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
// COMPLETENESS OR PERFORMANCE.
//===============================================================================

#include "include_qt4.h"
#include "Manager.h"

#include "ProcessDialog.h"

#include <exception>

using namespace wpkgar;

namespace
{
    class ProcessInterrupt : public wpkgar_interrupt
	{
		public:
			virtual bool stop_now()
			{
				return ProcessDialog::CancelClicked();
			}
	};

	ProcessInterrupt g_interrupt;
}


std::weak_ptr<Manager> Manager::f_instance;


Manager::Manager()
    : f_mutex( QMutex::Recursive )
{
    Init();
}


Manager::~Manager()
{
    f_installer.reset();
    f_remover.reset();
    f_lock.reset();
    f_manager.reset();
}


Manager::pointer_t Manager::WeakInstance()
{
    if( !f_instance.lock() )
    {
        pointer_t instance;
        instance.reset( new Manager );
        f_instance = instance;
        return instance;
    }
    return f_instance.lock();
}


#if 0
void Manager::Release()
{
    f_instance.reset();
}
#endif


bool Manager::InUse()
{
    return f_instance.use_count() > 0;
}


QMutex& Manager::GetMutex()
{
    return f_mutex;
}


std::weak_ptr<wpkgar::wpkgar_lock> Manager::GetLock()
{
    QMutexLocker locker( &f_mutex );
    return f_lock;
}


std::weak_ptr<wpkgar_manager> Manager::GetManager()
{
	QMutexLocker locker( &f_mutex );
    Q_ASSERT( f_manager );
    return f_manager;
}


std::weak_ptr<wpkgar_install>   Manager::GetInstaller()
{
	QMutexLocker locker( &f_mutex );
    if( !f_installer )
    {
        f_installer.reset( new wpkgar_install( f_manager.get() ) );
    }
    return f_installer;
}


std::weak_ptr<wpkgar_remove>   Manager::GetRemover()
{
    QMutexLocker locker( &f_mutex );
    if( !f_remover )
    {
        f_remover.reset( new wpkgar_remove( f_manager.get() ) );
    }
    return f_remover;
}


void Manager::ResetLock()
{
    Q_ASSERT( f_manager );
    f_manager->remove_lock();

    // Create the lock file by reinitializing
    //
    Init();
}


void Manager::Init()
{
	QMutexLocker locker( &f_mutex );

    f_logOutput = LogOutput::Instance();

    f_manager.reset( new wpkgar_manager );

    f_manager->set_interrupt_handler( &g_interrupt );

	// TODO: add the Qt packages we depend on once ready
	//       (specifically for MS-Windows)
    f_manager->add_self("wpkg-gui");
    f_manager->add_self("wpkgguiqt4");

    wpkg_output::set_output( f_logOutput.get() );
    f_logOutput->set_debug_flags( wpkg_output::debug_flags::debug_progress );

	QSettings settings;
	const QString root_path = settings.value( "root_path" ).toString();
    const QString database_path = QString("%1/var/lib/wpkg").arg(root_path);

    f_manager->set_root_path( root_path.toStdString() );
    f_manager->set_database_path( database_path.toStdString() );
	f_manager->add_sources_list();

    CreateLock();
}


void Manager::CreateLock()
{
    try
    {
        f_lock.reset( new wpkgar_lock( f_manager.get(), "Package Explorer" ) );
    }
    catch( const wpkgar_exception_locked& except )
    {
        f_logOutput->OutputToLog( wpkg_output::level_error, except.what() );
    }
    catch( const std::runtime_error& except )
    {
        f_logOutput->OutputToLog( wpkg_output::level_fatal, except.what() );
        qFatal( "Error when locking database: [%s]", except.what() );
    }
    catch( ... )
    {
        f_logOutput->OutputToLog( wpkg_output::level_fatal, "Unknown error!" );
        qFatal( "Unknown error when trying to lock database!" );
    }
}

// vim: ts=4 sw=4 et
