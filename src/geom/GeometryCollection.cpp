/**********************************************************************
 *
 * GEOS - Geometry Engine Open Source
 * http://geos.osgeo.org
 *
 * Copyright (C) 2001-2002 Vivid Solutions Inc.
 * Copyright (C) 2005 2006 Refractions Research Inc.
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU Lesser General Public Licence as published
 * by the Free Software Foundation.
 * See the COPYING file for more information.
 *
 **********************************************************************
 *
 * Last port: geom/GeometryCollection.java rev. 1.41
 *
 **********************************************************************/

#include <geos/geom/GeometryCollection.h>
#include <geos/util/IllegalArgumentException.h>
#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/CoordinateSequenceFilter.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/GeometryFilter.h>
#include <geos/geom/GeometryComponentFilter.h>
#include <geos/util.h>


#include <algorithm>
#include <vector>
#include <memory>

namespace geos {
namespace geom { // geos::geom

/*protected*/
GeometryCollection::GeometryCollection(const GeometryCollection& gc)
    :
    Geometry(gc),
    geometries(gc.geometries.size()),
    flags(gc.flags),
    envelope(gc.envelope)
{
    for(std::size_t i = 0; i < geometries.size(); ++i) {
        geometries[i] = gc.geometries[i]->clone();
    }
}

GeometryCollection&
GeometryCollection::operator=(const GeometryCollection& gc)
{
    geometries.resize(gc.geometries.size());
    envelope = gc.envelope;
    flags = gc.flags;

    for (std::size_t i = 0; i < geometries.size(); i++) {
        geometries[i] = gc.geometries[i]->clone();
    }

    return *this;
}

GeometryCollection::GeometryCollection(std::vector<std::unique_ptr<Geometry>> && newGeoms, const GeometryFactory& factory) :
    Geometry(&factory),
    geometries(std::move(newGeoms)),
    flags{}, // set all flags to zero
    envelope(computeEnvelopeInternal())
{

    if (hasNullElements(&geometries)) {
        throw util::IllegalArgumentException("geometries must not contain null elements\n");
    }

    setSRID(getSRID());
}

void
GeometryCollection::setSRID(int newSRID)
{
    Geometry::setSRID(newSRID);
    for(auto& g : geometries) {
        g->setSRID(newSRID);
    }
}

/*
 * Collects all coordinates of all subgeometries into a CoordinateSequence.
 *
 * Returns a newly the collected coordinates
 *
 */
std::unique_ptr<CoordinateSequence>
GeometryCollection::getCoordinates() const
{
    auto coordinates = detail::make_unique<CoordinateSequence>(getNumPoints());

    std::size_t k = 0;
    for(const auto& g : geometries) {
        auto childCoordinates = g->getCoordinates(); // TODO avoid this copy where getCoordinateRO() exists
        std::size_t npts = childCoordinates->getSize();
        for(std::size_t j = 0; j < npts; ++j) {
            coordinates->setAt(childCoordinates->getAt(j), k);
            k++;
        }
    }
    return coordinates;
}

bool
GeometryCollection::isEmpty() const
{
    for(const auto& g : geometries) {
        if(!g->isEmpty()) {
            return false;
        }
    }
    return true;
}

void
GeometryCollection::setFlags() const {
    if (flags.flagsCalculated) {
        return;
    }

    for (const auto& geom : geometries) {
        flags.hasPoints |= geom->hasDimension(Dimension::P);
        flags.hasLines |= geom->hasDimension(Dimension::L);
        flags.hasPolygons |= geom->hasDimension(Dimension::A);
        flags.hasM |= geom->hasM();
        flags.hasZ |= geom->hasZ();
        flags.hasCurves |= geom->hasCurvedComponents();
    }

    flags.flagsCalculated = true;
}

Dimension::DimensionType
GeometryCollection::getDimension() const
{
    setFlags();

    if (flags.hasPolygons) {
        return Dimension::A;
    }
    if (flags.hasLines) {
        return Dimension::L;
    }
    if (flags.hasPoints) {
        return Dimension::P;
    }
    return Dimension::False;
}

bool
GeometryCollection::isDimensionStrict(Dimension::DimensionType d) const {
    setFlags();

    if (isEmpty()) {
        return true;
    }

    switch(d) {
        case Dimension::A: return flags.hasPolygons && !flags.hasLines && !flags.hasPoints;
        case Dimension::L: return !flags.hasPolygons && flags.hasLines && !flags.hasPoints;
        case Dimension::P: return !flags.hasPolygons && !flags.hasLines && flags.hasPoints;
        default:
            return false;
    }
}

bool
GeometryCollection::hasDimension(Dimension::DimensionType d) const {
    setFlags();

    switch (d) {
        case Dimension:: A: return flags.hasPolygons;
        case Dimension:: L: return flags.hasLines;
        case Dimension:: P: return flags.hasPoints;
        default:
            return false;
    }
}

int
GeometryCollection::getBoundaryDimension() const
{
    int dimension = Dimension::False;
    for(const auto& g : geometries) {
        dimension = std::max(dimension, g->getBoundaryDimension());
    }
    return dimension;
}

uint8_t
GeometryCollection::getCoordinateDimension() const
{
    uint8_t dimension = 2;

    for(const auto& g : geometries) {
        dimension = std::max(dimension, g->getCoordinateDimension());
    }
    return dimension;
}

bool
GeometryCollection::hasM() const
{
    setFlags();
    return flags.hasM;
}

bool
GeometryCollection::hasZ() const
{
    setFlags();
    return flags.hasZ;
}

size_t
GeometryCollection::getNumGeometries() const
{
    return geometries.size();
}

const Geometry*
GeometryCollection::getGeometryN(std::size_t n) const
{
    return geometries[n].get();
}

std::vector<std::unique_ptr<Geometry>>
GeometryCollection::releaseGeometries()
{
    auto ret = std::move(geometries);
    geometryChanged();
    flags.flagsCalculated = false;
    return ret;
}

size_t
GeometryCollection::getNumPoints() const
{
    std::size_t numPoints = 0;
    for(const auto& g : geometries) {
        numPoints += g->getNumPoints();
    }
    return numPoints;
}

std::string
GeometryCollection::getGeometryType() const
{
    return "GeometryCollection";
}

std::unique_ptr<Geometry>
GeometryCollection::getBoundary() const
{
    throw util::IllegalArgumentException("Operation not supported by GeometryCollection\n");
}

bool
GeometryCollection::equalsExact(const Geometry* other, double tolerance) const
{
    if(!isEquivalentClass(other)) {
        return false;
    }

    const GeometryCollection* otherCollection = detail::down_cast<const GeometryCollection*>(other);
    if(geometries.size() != otherCollection->geometries.size()) {
        return false;
    }
    for(std::size_t i = 0; i < geometries.size(); ++i) {
        if(!(geometries[i]->equalsExact(otherCollection->geometries[i].get(), tolerance))) {
            return false;
        }
    }
    return true;
}

bool
GeometryCollection::equalsIdentical(const Geometry* other_g) const
{
    if(!isEquivalentClass(other_g)) {
        return false;
    }

    const auto& other = static_cast<const GeometryCollection&>(*other_g);
    if(getNumGeometries() != other.getNumGeometries()) {
        return false;
    }

    if (envelope != other.envelope) {
        return false;
    }

    for(std::size_t i = 0; i < getNumGeometries(); i++) {
        if(!(getGeometryN(i)->equalsIdentical(other.getGeometryN(i)))) {
            return false;
        }
    }

    return true;
}

void
GeometryCollection::apply_rw(const CoordinateFilter* filter)
{
    for(auto& g : geometries) {
        g->apply_rw(filter);
    }
}

void
GeometryCollection::apply_ro(CoordinateFilter* filter) const
{
    for(const auto& g : geometries) {
        g->apply_ro(filter);
    }
}

void
GeometryCollection::apply_ro(GeometryFilter* filter) const
{
    filter->filter_ro(this);
    for(const auto& g : geometries) {
        g->apply_ro(filter);
    }
}

void
GeometryCollection::apply_rw(GeometryFilter* filter)
{
    filter->filter_rw(this);
    for(auto& g : geometries) {
        g->apply_rw(filter);
    }
}

void
GeometryCollection::normalize()
{
    for(auto& g : geometries) {
        g->normalize();
    }
    std::sort(geometries.begin(), geometries.end(), [](const std::unique_ptr<Geometry> & a, const std::unique_ptr<Geometry> & b) {
        return a->compareTo(b.get()) > 0;
    });
}

Envelope
GeometryCollection::computeEnvelopeInternal() const
{
    Envelope p_envelope;
    for(const auto& g : geometries) {
        const Envelope* env = g->getEnvelopeInternal();
        p_envelope.expandToInclude(env);
    }
    return p_envelope;
}

int
GeometryCollection::compareToSameClass(const Geometry* g) const
{
    const GeometryCollection* gc = detail::down_cast<const GeometryCollection*>(g);
    return compare(geometries, gc->geometries);
}

bool GeometryCollection::hasCurvedComponents() const {
    setFlags();
    return flags.hasCurves;
}

const CoordinateXY*
GeometryCollection::getCoordinate() const
{
    for(const auto& g : geometries) {
        if(!g->isEmpty()) {
            return g->getCoordinate();
        }
    }
    return nullptr;
}

/**
 * @return the area of this collection
 */
double
GeometryCollection::getArea() const
{
    double area = 0.0;
    for(const auto& g : geometries) {
        area += g->getArea();
    }
    return area;
}

/**
 * @return the total length of this collection
 */
double
GeometryCollection::getLength() const
{
    double sum = 0.0;
    for(const auto& g : geometries) {
        sum += g->getLength();
    }
    return sum;
}

void
GeometryCollection::apply_rw(GeometryComponentFilter* filter)
{
    filter->filter_rw(this);
    for(auto& g : geometries) {
        if (filter->isDone()) {
            return;
        }
        g->apply_rw(filter);
    }
}

void
GeometryCollection::apply_ro(GeometryComponentFilter* filter) const
{
    filter->filter_ro(this);
    for(const auto& g : geometries) {
        if (filter->isDone()) {
            return;
        }
        g->apply_ro(filter);
    }
}

void
GeometryCollection::apply_rw(CoordinateSequenceFilter& filter)
{
    for(auto& g : geometries) {
        g->apply_rw(filter);
        if(filter.isDone()) {
            break;
        }
    }
    if(filter.isGeometryChanged()) {
        geometryChanged();
    }
}

void
GeometryCollection::apply_ro(CoordinateSequenceFilter& filter) const
{
    for(const auto& g : geometries) {
        g->apply_ro(filter);
        if(filter.isDone()) {
            break;
        }
    }

    assert(!filter.isGeometryChanged()); // read-only filter...
    //if (filter.isGeometryChanged()) geometryChanged();
}

GeometryTypeId
GeometryCollection::getGeometryTypeId() const
{
    return GEOS_GEOMETRYCOLLECTION;
}

GeometryCollection*
GeometryCollection::reverseImpl() const
{
    if(isEmpty()) {
        return clone().release();
    }

    std::vector<std::unique_ptr<Geometry>> reversed(geometries.size());

    std::transform(geometries.begin(),
                   geometries.end(),
                   reversed.begin(),
    [](const std::unique_ptr<Geometry> & g) {
        return g->reverse();
    });

    return getFactory()->createGeometryCollection(std::move(reversed)).release();
}



} // namespace geos::geom
} // namespace geos
