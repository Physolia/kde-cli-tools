/*
 *  Copyright (C) 2002, 2003 David Faure   <faure@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */


#include <kmimetypetrader.h>
#include <kservicetypetrader.h>

#include <stdio.h>
#include <QCoreApplication>
#include <KAboutData>
#include <KLocalizedString>
#include <QCommandLineParser>
#include <QCommandLineOption>

int main( int argc, char **argv )
{
    QCoreApplication app(argc, argv);

    KAboutData aboutData(QLatin1String("ktraderclient"), i18n("KTraderClient"), QLatin1String(PROJECT_VERSION));
    aboutData.addAuthor(i18n("David Faure"), QString(), "faure@kde.org");

    aboutData.setShortDescription(i18n("A command-line tool for querying the KDE trader system"));
    QCommandLineParser parser;
    KAboutData::setApplicationData(aboutData);
    parser.addVersionOption();
    parser.addHelpOption();
    aboutData.setupCommandLine(&parser);

    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("mimetype"), i18n("A mimetype"), QLatin1String("mimetype")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("servicetype"), i18n("A servicetype, like KParts/ReadOnlyPart or KMyApp/Plugin"), QLatin1String("servicetype")));
    parser.addOption(QCommandLineOption(QStringList() << QLatin1String("constraint"), i18n("A constraint expressed in the trader query language"), QLatin1String("constraint")));

    parser.process(app);
    aboutData.processCommandLine(&parser);

    const QString mimetype = parser.value("mimetype");
    QString servicetype = parser.value("servicetype");
    const QString constraint = parser.value("constraint");

  if ( mimetype.isEmpty() && servicetype.isEmpty() )
      parser.showHelp();

  if ( !mimetype.isEmpty() )
      printf( "mimetype is : %s\n", qPrintable( mimetype ) );
  if ( !servicetype.isEmpty() )
      printf( "servicetype is : %s\n", qPrintable( servicetype ) );
  if ( !constraint.isEmpty() )
      printf( "constraint is : %s\n", qPrintable( constraint ) );

  KService::List offers;
  if ( !mimetype.isEmpty() ) {
      if ( servicetype.isEmpty() )
          servicetype = "Application";
     offers = KMimeTypeTrader::self()->query( mimetype, servicetype, constraint );
  }
  else
     offers = KServiceTypeTrader::self()->query( servicetype, constraint );

  printf("got %d offers.\n", offers.count());

  int i = 0;
  KService::List::ConstIterator it = offers.constBegin();
  const KService::List::ConstIterator end = offers.constEnd();
  for (; it != end; ++it, ++i )
  {
    printf("---- Offer %d ----\n", i);
    QStringList props = (*it)->propertyNames();
    QStringList::ConstIterator propIt = props.constBegin();
    QStringList::ConstIterator propEnd = props.constEnd();
    for (; propIt != propEnd; ++propIt )
    {
      QVariant prop = (*it)->property( *propIt );

      if ( !prop.isValid() )
      {
        printf("Invalid property %s\n", (*propIt).toLocal8Bit().data());
        continue;
      }

      QString outp = *propIt;
      outp += " : '";

      switch ( prop.type() )
      {
        case QVariant::StringList:
          outp += prop.toStringList().join(" - ");
        break;
        case QVariant::Bool:
          outp += prop.toBool() ? "TRUE" : "FALSE";
          break;
        default:
          outp += prop.toString();
        break;
      }

      if ( !outp.isEmpty() )
        printf("%s'\n", outp.toLocal8Bit().data());
    }
  }
  return 0;
}

