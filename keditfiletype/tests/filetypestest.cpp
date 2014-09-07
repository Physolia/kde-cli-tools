/* This file is part of the KDE project
    Copyright 2007 David Faure <faure@kde.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License or ( at
   your option ) version 3 or, at the discretion of KDE e.V. ( which shall
   act as a proxy as in section 14 of the GPLv3 ), any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
   Boston, MA 02110-1301, USA.
*/

#include <kprocess.h>
#include <kservice.h>

#include <kconfiggroup.h>
#include <kdebug.h>
#include <kdesktopfile.h>
#include <ksycoca.h>

// Qt
#include <QDir>
#include <QStandardPaths>
#include <QTest>
#include <QSignalSpy>

#include <mimetypedata.h>
#include <mimetypewriter.h>

#define KDE_MAKE_VERSION( a,b,c ) (((a) << 16) | ((b) << 8) | (c))


class FileTypesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        extern KSERVICE_EXPORT bool kservice_require_kded;
        kservice_require_kded = false;

        QStandardPaths::setTestModeEnabled(true);

        m_mimeTypeCreatedSuccessfully = false;
        QStringList appsDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
        //kDebug() << appsDirs;
        m_localApps = appsDirs.first() + '/';
        m_localConfig = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        QVERIFY(QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/mime/packages")));

        QFile::remove(m_localConfig + "mimeapps.list");

        // Create fake applications for some tests below.
        bool mustUpdateKSycoca = false;
        fakeApplication = "fakeapplication.desktop";
        if (createDesktopFile(m_localApps + fakeApplication))
            mustUpdateKSycoca = true;
        fakeApplication2 = "fakeapplication2.desktop";
        if (createDesktopFile(m_localApps + fakeApplication2))
            mustUpdateKSycoca = true;

        // Cleanup after testMimeTypePatterns if it failed mid-way
        const QString packageFileName = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/mime/") + "packages/text-plain.xml" ;
        if (!packageFileName.isEmpty()) {
            QFile::remove(packageFileName);
            MimeTypeWriter::runUpdateMimeDatabase();
            mustUpdateKSycoca = true;
        }

        QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1Char('/') + "filetypesrc");

        if ( mustUpdateKSycoca ) {
            // Update ksycoca in ~/.kde-unit-test after creating the above
            runKBuildSycoca();
        }
        KService::Ptr fakeApplicationService = KService::serviceByStorageId(fakeApplication);
        QVERIFY(fakeApplicationService);
    }

    void testMimeTypeGroupAutoEmbed()
    {
        MimeTypeData data("text");
        QCOMPARE(data.majorType(), QString("text"));
        QCOMPARE(data.name(), QString("text"));
        QVERIFY(data.isMeta());
        QCOMPARE(data.autoEmbed(), MimeTypeData::No); // text doesn't autoembed by default
        QVERIFY(!data.isDirty());
        data.setAutoEmbed(MimeTypeData::Yes);
        QCOMPARE(data.autoEmbed(), MimeTypeData::Yes);
        QVERIFY(data.isDirty());
        QVERIFY(!data.sync()); // save to disk. Should succeed, but return false (no need to run update-mime-database)
        QVERIFY(!data.isDirty());
        // Check what's on disk by creating another MimeTypeData instance
        MimeTypeData data2("text");
        QCOMPARE(data2.autoEmbed(), MimeTypeData::Yes);
        QVERIFY(!data2.isDirty());
        data2.setAutoEmbed(MimeTypeData::No); // revert to default, for next time
        QVERIFY(data2.isDirty());
        QVERIFY(!data2.sync());
        QVERIFY(!data2.isDirty());

        // TODO test askSave after cleaning up the code
    }

    void testMimeTypeAutoEmbed()
    {
        QMimeDatabase db;
        MimeTypeData data(db.mimeTypeForName("text/plain"));
        QCOMPARE(data.majorType(), QString("text"));
        QCOMPARE(data.minorType(), QString("plain"));
        QCOMPARE(data.name(), QString("text/plain"));
        QVERIFY(!data.isMeta());
        QCOMPARE(data.autoEmbed(), MimeTypeData::UseGroupSetting);
        QVERIFY(!data.isDirty());
        data.setAutoEmbed(MimeTypeData::Yes);
        QCOMPARE(data.autoEmbed(), MimeTypeData::Yes);
        QVERIFY(data.isDirty());
        QVERIFY(!data.sync()); // save to disk. Should succeed, but return false (no need to run update-mime-database)
        QVERIFY(!data.isDirty());
        // Check what's on disk by creating another MimeTypeData instance
        MimeTypeData data2(db.mimeTypeForName("text/plain"));
        QCOMPARE(data2.autoEmbed(), MimeTypeData::Yes);
        QVERIFY(!data2.isDirty());
        data2.setAutoEmbed(MimeTypeData::UseGroupSetting); // revert to default, for next time
        QVERIFY(data2.isDirty());
        QVERIFY(!data2.sync());
        QVERIFY(!data2.isDirty());
    }

    void testMimeTypePatterns()
    {
        QMimeDatabase db;
        MimeTypeData data(db.mimeTypeForName("text/plain"));
        QCOMPARE(data.name(), QString("text/plain"));
        QCOMPARE(data.majorType(), QString("text"));
        QCOMPARE(data.minorType(), QString("plain"));
        QVERIFY(!data.isMeta());
        QStringList patterns = data.patterns();
        QVERIFY(patterns.contains("*.txt"));
        QVERIFY(!patterns.contains("*.toto"));
        const QStringList origPatterns = patterns;
        patterns.removeAll("*.txt");
        patterns.append("*.toto"); // yes, a french guy wrote this, as you can see
        patterns.sort(); // for future comparisons
        QVERIFY(!data.isDirty());
        data.setPatterns(patterns);
        QVERIFY(data.isDirty());
        bool needUpdateMimeDb = data.sync();
        QVERIFY(needUpdateMimeDb);
        MimeTypeWriter::runUpdateMimeDatabase();
        //runKBuildSycoca();
        QCOMPARE(data.patterns(), patterns);
        data.refresh(); // reload from the xml
        QCOMPARE(data.patterns(), patterns);
        // Check what's in ksycoca
        QStringList newPatterns = db.mimeTypeForName("text/plain").globPatterns();
        newPatterns.sort();
        QCOMPARE(newPatterns, patterns);
        QVERIFY(!data.isDirty());

        // Remove custom file
        const QString packageFileName = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/mime/") + "packages/text-plain.xml" ;
        QVERIFY(!packageFileName.isEmpty());
        QFile::remove(packageFileName);
        MimeTypeWriter::runUpdateMimeDatabase();
        //runKBuildSycoca();
        // Check what's in ksycoca
        newPatterns = db.mimeTypeForName("text/plain").globPatterns();
        newPatterns.sort();
        QCOMPARE(newPatterns, origPatterns);
    }

    void testAddService()
    {
        QMimeDatabase db;
        const char* mimeTypeName = "application/rtf"; // use inherited mimetype to test #321706
        MimeTypeData data(db.mimeTypeForName(mimeTypeName));
        QStringList appServices = data.appServices();
        //kDebug() << appServices;
        QVERIFY(!appServices.isEmpty());
        const QString oldPreferredApp = appServices.first();
        QVERIFY(!appServices.contains(fakeApplication)); // already there? hmm can't really test then
        QVERIFY(!data.isDirty());
        appServices.prepend(fakeApplication);
        data.setAppServices(appServices);
        QVERIFY(data.isDirty());
        QVERIFY(!data.sync()); // success, but no need to run update-mime-database
        runKBuildSycoca();
        QVERIFY(!data.isDirty());
        // Check what's in ksycoca
        checkMimeTypeServices(mimeTypeName, appServices);
        // Check what's in mimeapps.list
        checkAddedAssociationsContains(mimeTypeName, fakeApplication);

        // Test reordering apps, i.e. move fakeApplication under oldPreferredApp
        appServices.removeFirst();
        appServices.insert(1, fakeApplication);
        data.setAppServices(appServices);
        QVERIFY(!data.sync()); // success, but no need to run update-mime-database
        runKBuildSycoca();
        QVERIFY(!data.isDirty());
        // Check what's in ksycoca
        checkMimeTypeServices(mimeTypeName, appServices);
        // Check what's in mimeapps.list
        checkAddedAssociationsContains(mimeTypeName, fakeApplication);

        // Now test removing (in the same test, since it's inter-dependent)
        QVERIFY(appServices.removeAll(fakeApplication) > 0);
        data.setAppServices(appServices);
        QVERIFY(data.isDirty());
        QVERIFY(!data.sync()); // success, but no need to run update-mime-database
        runKBuildSycoca();
        // Check what's in ksycoca
        checkMimeTypeServices(mimeTypeName, appServices);
        // Check what's in mimeapps.list
        checkRemovedAssociationsContains(mimeTypeName, fakeApplication);
    }

    void testRemoveTwice()
    {
        QMimeDatabase db;
        // Remove fakeApplication from image/png
        const char* mimeTypeName = "image/png";
        MimeTypeData data(db.mimeTypeForName(mimeTypeName));
        QStringList appServices = data.appServices();
        kDebug() << "initial list for" << mimeTypeName << appServices;
        QVERIFY(appServices.removeAll(fakeApplication) > 0);
        data.setAppServices(appServices);
        QVERIFY(!data.sync()); // success, but no need to run update-mime-database
        runKBuildSycoca();
        // Check what's in ksycoca
        checkMimeTypeServices(mimeTypeName, appServices);
        // Check what's in mimeapps.list
        checkRemovedAssociationsContains(mimeTypeName, fakeApplication);

        // Remove fakeApplication2 from image/png; must keep the previous entry in "Removed Associations"
        kDebug() << "Removing fakeApplication2";
        QVERIFY(appServices.removeAll(fakeApplication2) > 0);
        data.setAppServices(appServices);
        QVERIFY(!data.sync()); // success, but no need to run update-mime-database
        runKBuildSycoca();
        // Check what's in ksycoca
        checkMimeTypeServices(mimeTypeName, appServices);
        // Check what's in mimeapps.list
        checkRemovedAssociationsContains(mimeTypeName, fakeApplication);
        // Check what's in mimeapps.list
        checkRemovedAssociationsContains(mimeTypeName, fakeApplication2);

        // And now re-add fakeApplication2...
        kDebug() << "Re-adding fakeApplication2";
        appServices.prepend(fakeApplication2);
        data.setAppServices(appServices);
        QVERIFY(!data.sync()); // success, but no need to run update-mime-database
        runKBuildSycoca();
        // Check what's in ksycoca
        checkMimeTypeServices(mimeTypeName, appServices);
        // Check what's in mimeapps.list
        checkRemovedAssociationsContains(mimeTypeName, fakeApplication);
        checkRemovedAssociationsDoesNotContain(mimeTypeName, fakeApplication2);
    }

    void testCreateMimeType()
    {
        QMimeDatabase db;
        const QString mimeTypeName = "fake/unit-test-fake-mimetype";
        // Clean up after previous runs if necessary
        if (MimeTypeWriter::hasDefinitionFile(mimeTypeName))
            MimeTypeWriter::removeOwnMimeType(mimeTypeName);

        MimeTypeData data(mimeTypeName, true);
        data.setComment("Fake MimeType");
        QStringList patterns = QStringList() << "*.pkg.tar.gz";
        data.setPatterns(patterns);
        QVERIFY(data.isDirty());
        QVERIFY(data.sync());
        MimeTypeWriter::runUpdateMimeDatabase();
        //runKBuildSycoca();
        QMimeType mime = db.mimeTypeForName(mimeTypeName);
        QVERIFY(mime.isValid());
        QCOMPARE(mime.comment(), QString("Fake MimeType"));
        QCOMPARE(mime.globPatterns(), patterns); // must sort them if more than one

        // Testcase for the shaman.xml bug
        QCOMPARE(db.mimeTypeForFile("/whatever/foo.pkg.tar.gz").name(), QString("fake/unit-test-fake-mimetype"));

        m_mimeTypeCreatedSuccessfully = true;
    }

    void testDeleteMimeType()
    {
        QMimeDatabase db;
        if (!m_mimeTypeCreatedSuccessfully)
            QSKIP("This test relies on testCreateMimeType");
        const QString mimeTypeName = "fake/unit-test-fake-mimetype";
        QVERIFY(MimeTypeWriter::hasDefinitionFile(mimeTypeName));
        MimeTypeWriter::removeOwnMimeType(mimeTypeName);
        MimeTypeWriter::runUpdateMimeDatabase();
        //runKBuildSycoca();
        QMimeType mime = db.mimeTypeForName(mimeTypeName);
        QVERIFY(mime.isValid());
    }

    void testModifyMimeTypeComment() // of a system mimetype. And check that it's re-read correctly.
    {
        QMimeDatabase db;
        const char* mimeTypeName = "image/png";
        MimeTypeData data(db.mimeTypeForName(mimeTypeName));
        QCOMPARE(data.comment(), QString::fromLatin1("PNG image"));
        const char* fakeComment = "PNG image [testing]";
        data.setComment(fakeComment);
        QVERIFY(data.isDirty());
        QVERIFY(data.sync());
        MimeTypeWriter::runUpdateMimeDatabase();
        //runKBuildSycoca();
        QMimeType mime = db.mimeTypeForName(mimeTypeName);
        QVERIFY(mime.isValid());
        QCOMPARE(mime.comment(), QString::fromLatin1(fakeComment));

        // Cleanup
        QVERIFY(MimeTypeWriter::hasDefinitionFile(mimeTypeName));
        MimeTypeWriter::removeOwnMimeType(mimeTypeName);
    }

    void cleanupTestCase()
    {
        // If we remove it, then every run of the unit test has to run kbuildsycoca... slow.
        //QFile::remove(QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + QLatin1Char('/') + "fakeapplication.desktop");
    }

private: // helper methods

    void checkAddedAssociationsContains(const QString& mimeTypeName, const QString& application)
    {
        const KConfig config(m_localConfig + "mimeapps.list", KConfig::NoGlobals);
        const KConfigGroup group(&config, "Added Associations");
        const QStringList addedEntries = group.readXdgListEntry(mimeTypeName);
        if (!addedEntries.contains(application)) {
            kWarning() << addedEntries << "does not contain" << application;
            QVERIFY(addedEntries.contains(application));
        }
    }

    void checkRemovedAssociationsContains(const QString& mimeTypeName, const QString& application)
    {
        const KConfig config(m_localConfig + "mimeapps.list", KConfig::NoGlobals);
        const KConfigGroup group(&config, "Removed Associations");
        const QStringList removedEntries = group.readXdgListEntry(mimeTypeName);
        if (!removedEntries.contains(application)) {
            kWarning() << removedEntries << "does not contain" << application;
            QVERIFY(removedEntries.contains(application));
        }
    }

    void checkRemovedAssociationsDoesNotContain(const QString& mimeTypeName, const QString& application)
    {
        const KConfig config(m_localConfig + "mimeapps.list", KConfig::NoGlobals);
        const KConfigGroup group(&config, "Removed Associations");
        const QStringList removedEntries = group.readXdgListEntry(mimeTypeName);
        if (removedEntries.contains(application)) {
            kWarning() << removedEntries << "contains" << application;
            QVERIFY(!removedEntries.contains(application));
        }
    }

    void runKBuildSycoca()
    {
        // Wait for notifyDatabaseChanged DBus signal
        // (The real KCM code simply does the refresh in a slot, asynchronously)

        QProcess proc;
        //proc.setProcessChannelMode(QProcess::ForwardedChannels);
        const QString kbuildsycoca = QStandardPaths::findExecutable(KBUILDSYCOCA_EXENAME);
        QVERIFY(!kbuildsycoca.isEmpty());
        QStringList args;
        args << "--testmode";
        proc.start(kbuildsycoca, args);
        QSignalSpy spy(KSycoca::self(), SIGNAL(databaseChanged(QStringList)));
        proc.waitForFinished();
        qDebug() << "waiting for signal";
        QVERIFY(spy.wait(10000));
        qDebug() << "got signal";
    }

    bool createDesktopFile(const QString& path)
    {
        if (!QFile::exists(path)) {
            KDesktopFile file(path);
            KConfigGroup group = file.desktopGroup();
            group.writeEntry("Name", "FakeApplication");
            group.writeEntry("Type", "Application");
            group.writeEntry("Exec", "ls");
            group.writeEntry("MimeType", "image/png");
            return true;
        }
        return false;
    }

    void checkMimeTypeServices(const QString& mimeTypeName, const QStringList& expectedServices)
    {
        QMimeDatabase db;
        MimeTypeData data2(db.mimeTypeForName(mimeTypeName));
        if (data2.appServices() != expectedServices)
            kDebug() << "got" << data2.appServices() << "expected" << expectedServices;
        QCOMPARE(data2.appServices(), expectedServices);
    }

    QString fakeApplication; // storage id of the fake application
    QString fakeApplication2; // storage id of the fake application2
    QString m_localApps;
    QString m_localConfig;
    bool m_mimeTypeCreatedSuccessfully;
};

QTEST_MAIN(FileTypesTest)

#include "filetypestest.moc"
